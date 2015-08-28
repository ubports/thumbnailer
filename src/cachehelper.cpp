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

#include <internal/cachehelper.h>

#include <boost/filesystem.hpp>
#include <QDebug>

#include <system_error>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

// Simple call wrapper to allow us to call a functor and
// check if the called function threw an exception. If so, we re-try
// the call after calling recover(). In turn, if the exception
// indicates DB corruption, it deletes the cache, re-initializes it,
// and re-tries the function call one more time.

template<typename T>
T CacheHelper::call(std::function<T(void)> func) const
{
    try
    {
        return func();  // Try and call the passed function.
    }
    catch (...)
    {
        recover();      // If the DB is corrupt, recover() wipes the DB. If not, it re-throws.
        return func();  // Try again with the recovered DB.
    }
}

// Specialization for void return type.

template<>
void CacheHelper::call(function<void(void)> func) const
{
    try
    {
        func();      // Try and call the passed function.
    }
    catch (...)
    {
        recover();  // If the DB is corrupt, recover() wipes the DB. If not, it re-throws.
        func();     // Try again with the recovered DB.
    }
}

CacheHelper::CacheHelper(string const& cache_path, int64_t max_size_in_bytes, core::CacheDiscardPolicy policy)
    : path_(cache_path)
    , size_(max_size_in_bytes)
    , policy_(policy)
{
    call<void>([&]{ init_cache(); });
}

core::Optional<string> CacheHelper::get(string const& key) const
{
    return call<core::Optional<string>>([&]{ return c_->get(key); });
}

bool CacheHelper::put(string const& key, string const& value, chrono::time_point<chrono::system_clock> expiry_time)
{
    return call<bool>([&]{ return c_->put(key,value, expiry_time); });
}

core::PersistentCacheStats CacheHelper::stats() const
{
    return c_->stats();
}

void CacheHelper::clear_stats()
{
    c_->clear_stats();
}

void CacheHelper::invalidate()
{
    call<void>([&]{ c_->invalidate(); });
}

void CacheHelper::compact()
{
    call<void>([&]{ c_->compact(); });
}

// Called if a call on the PersistentStringCache throws an exception.
// If the exception was not a system_error, or was a system error with
// any code other than 666 we just let it escape. Otherwise, if the
// exception was a system error with code 666, leveldb detected
// DB corruption, and we delete the physical DB files on disk
// and re-initialize the DB.

void CacheHelper::recover() const
{
    exception_ptr e = current_exception();
    try
    {
        rethrow_exception(e);
    }
    catch (system_error const& e)
    {
        if (e.code().value() != 666)
        {
            // Not a database corruption error.
            throw;
        }

        // DB is corrupt. Blow away the cache directory and re-initialize the cache.
        qDebug().nospace() << "CacheHelper: corrupt database: " << e.what()
                           << "\n    deleting contents of " << QString::fromStdString(path_);
        try
        {
            c_.reset();
            boost::filesystem::remove_all(path_);
            const_cast<CacheHelper*>(this)->init_cache();
        }
        catch (...)
        {
            // Deliberately ignored. If it didn't work, we'll find out on the retry.
        }
    }
}

void CacheHelper::init_cache()
{
    try
    {
        c_ = move(core::PersistentStringCache::open(path_, size_, policy_));
    }
    catch (logic_error const&)
    {
        // Cache size has changed.
        c_ = move(core::PersistentStringCache::open(path_));
        c_->resize(size_);
    }
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
