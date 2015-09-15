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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 */

#include "supports_decoder.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#include <gst/gst.h>
#pragma GCC diagnostic pop

#include <memory>
#include <set>

bool supports_decoder(const std::string& format)
{
    static std::set<std::string> formats;

    if (formats.empty())
    {
        std::unique_ptr<GList, decltype(&gst_plugin_feature_list_free)> decoders(
            gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER, GST_RANK_NONE),
            gst_plugin_feature_list_free);
        for (const GList* l = decoders.get(); l != nullptr; l = l->next)
        {
            const auto factory = static_cast<GstElementFactory*>(l->data);

            const GList* templates = gst_element_factory_get_static_pad_templates(factory);
            for (const GList* l = templates; l != nullptr; l = l->next)
            {
                const auto t = static_cast<GstStaticPadTemplate*>(l->data);
                if (t->direction != GST_PAD_SINK)
                {
                    continue;
                }

                std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> caps(gst_static_caps_get(&t->static_caps),
                                                                         gst_caps_unref);
                for (unsigned int i = 0; i < gst_caps_get_size(caps.get()); i++)
                {
                    const auto structure = gst_caps_get_structure(caps.get(), i);
                    formats.emplace(gst_structure_get_name(structure));
                }
            }
        }
    }

    return formats.find(format) != formats.end();
}
