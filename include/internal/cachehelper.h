/*
 * Copyright (C) 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#pragma once

#include <core/persistent_string_cache.h>

#include <boost/filesystem.hpp>
#include <QDebug>

#include <system_error>

namespace unity
{

namespace thumbnailer
{

namespace internal
{

// Helper class to wrap access to a persistent cache. We use this
// to handle database corruption: if the DB reports that it is corrupt
// with code 666), we delete the cache files and re-create the cache,
// then retry the call one more time.
//
// In addition, the constructor also deals with caches that are re-sized when opened.
//
// This is a template so we can inject a mock cache for testing.

template<typename CacheT>
class CacheHelper final
{
public:
    typedef CacheT type;

    using UPtr = std::unique_ptr<CacheHelper<CacheT>>;  // Convenience definition for clients.

    CacheT& cache() const
    {
        return *c_;
    }

    static typename CacheHelper<CacheT>::UPtr open(std::string const& cache_path,
                                                   int64_t max_size_in_bytes,
                                                   core::CacheDiscardPolicy policy);
    static typename CacheHelper<CacheT>::UPtr open(std::string const& cache_path);

    // Methods below pass through to the underlying cache, but with retry after
    // recovery if the underlying cache reports a corrupt DB.
    core::Optional<std::string> get(std::string const& key) const;
    bool put(std::string const& key,
             std::string const& value,
             std::chrono::time_point<std::chrono::system_clock> expiry_time = std::chrono::system_clock::time_point());
    core::PersistentCacheStats stats() const;
    void clear_stats();
    void invalidate();
    void compact();

private:
    CacheHelper<CacheT>(std::string const& cache_path,
                        int64_t max_size_in_bytes,
                        core::CacheDiscardPolicy policy);
    CacheHelper<CacheT>(std::string const& cache_path);

    // Call wrapper that implements the retry logic.
    template<typename T>
    T call(std::function<T(void)> func) const
    {
        try
        {
            return func();  // Try and call the passed function.
        }
        catch (...)
        {
            // If the DB is corrupt, recover() wipes the DB. If DB is not corrupt,
            // and there was some other error, it re-throws.
            recover();
            qDebug() << "CacheHelper: reinitialized cache, retrying failed operation";
            try
            {
                return func();  // Try again with the recovered DB.
            }
            catch (std::exception const& e)
            {
                qCritical() << "CacheHelper: retry failed:" << e.what();
                throw;
            }
            catch (...)
            {
                qCritical() << "CacheHelper: retry failed: unknown exception";
                throw;
            }
        }
    }

    void recover() const;
    void init_cache();

    std::string const path_;
    int64_t const size_;
    core::CacheDiscardPolicy const policy_;
    mutable std::unique_ptr<CacheT> c_;
};

// Convenience definition for the normal use case with a real cache.
using PersistentCacheHelper = CacheHelper<core::PersistentStringCache>;

template<typename CacheT>
inline
typename CacheHelper<CacheT>::UPtr CacheHelper<CacheT>::open(std::string const& cache_path,
                                                             int64_t max_size_in_bytes,
                                                             core::CacheDiscardPolicy policy)
{
    return UPtr(new CacheHelper<CacheT>(cache_path, max_size_in_bytes, policy));
}

template<typename CacheT>
inline
typename CacheHelper<CacheT>::UPtr CacheHelper<CacheT>::open(std::string const& cache_path)
{
    return UPtr(new CacheHelper<CacheT>(cache_path));
}

template<typename CacheT>
inline
CacheHelper<CacheT>::CacheHelper(std::string const& cache_path,
                                 int64_t max_size_in_bytes,
                                 core::CacheDiscardPolicy policy)
    : path_(cache_path)
    , size_(max_size_in_bytes)
    , policy_(policy)
{
    call<void>([&]{ init_cache(); });
}

template<typename CacheT>
inline
CacheHelper<CacheT>::CacheHelper(std::string const& cache_path)
    : path_(cache_path)
    , c_(move(CacheT::open(path_)))
{
    auto stats = c_->stats();
    size_ = stats->max_size_in_bytes();
    policy_ = stats->policy();
}

template<typename CacheT>
inline
core::Optional<std::string> CacheHelper<CacheT>::get(std::string const& key) const
{
    return call<core::Optional<std::string>>([&]{ return c_->get(key); });
}

template<typename CacheT>
inline
bool CacheHelper<CacheT>::put(std::string const& key,
                              std::string const& value,
                              std::chrono::time_point<std::chrono::system_clock> expiry_time)
{
    return call<bool>([&]{ return c_->put(key, value, expiry_time); });
}

template<typename CacheT>
inline
core::PersistentCacheStats CacheHelper<CacheT>::stats() const
{
    return c_->stats();
}

template<typename CacheT>
inline
void CacheHelper<CacheT>::clear_stats()
{
    c_->clear_stats();
}

template<typename CacheT>
inline
void CacheHelper<CacheT>::invalidate()
{
    call<void>([&]{ c_->invalidate(); });
}

template<typename CacheT>
inline
void CacheHelper<CacheT>::compact()
{
    call<void>([&]{ c_->compact(); });
}

// Called if a call on the underlying cache throws an exception.
// If the exception was not a system_error, or was a system error with
// any code other than 666, we just let it escape. Otherwise, if the
// exception was a system error with code 666, leveldb detected
// DB corruption, and we delete the physical DB files and reinitialize the DB.

template<typename CacheT>
void CacheHelper<CacheT>::recover() const
{
    using namespace std;

    exception_ptr e = current_exception();
    assert(e);
    try
    {
        rethrow_exception(e);
    }
    catch (system_error const& se)
    {
        if (se.code().value() != 666)
        {
            // Not a database corruption error.
            throw;
        }

        // DB is corrupt. Blow away the cache directory and reinitialize the cache.
        qCritical() << "CacheHelper: corrupt database:" << se.what()
                    << ": deleting" << QString::fromStdString(path_);
        try
        {
            c_.reset();
            boost::filesystem::remove_all(path_);
            const_cast<CacheHelper*>(this)->init_cache();
        }
        catch (std::exception const& inner)
        {
            qCritical() << "CacheHelper: error during recovery:" << inner.what();
            throw;
        }
        catch (...)
        {
            qCritical() << "CacheHelper: error during recovery: unknown exception";
            throw;
        }
    }
}

// Helper to initialize a cache. If an existing cache is opened with
// a different size, we re-size the cache automatically.

template<typename CacheT>
void CacheHelper<CacheT>::init_cache()
{
using namespace std;
    try
    {
        c_ = move(CacheT::open(path_, size_, policy_));
    }
    catch (std::logic_error const&)
    {
        // Cache size has changed.
        c_ = move(CacheT::open(path_));
        c_->resize(size_);
    }
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
