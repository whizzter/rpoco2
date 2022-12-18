#pragma once

#include "json_std_string.hpp"
#include <map>

namespace rpoco2 {
	namespace json {
		namespace impl {
			template<typename V, class CTX>
			struct parser<std::map<std::string,V>, CTX> {
				static bool parse(std::map<std::string,V> &v, CTX& ctx) {
					std::string key;
					auto keyFn =
						[&key](CTX& ctx) {
						key.clear();
						return parser<std::string, CTX>::parse(key, ctx);
					};
					auto valFn = [&key, &v](CTX& ctx) {
						return parser<V, CTX>::parse(v[key], ctx);
					};
					return parse_object(keyFn,valFn,ctx
					);
				}
			};
		}
	}
}

