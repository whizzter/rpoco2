#pragma once

#include "json.hpp"
#include <vector>

namespace rpoco2 {
	namespace json {
		namespace impl {
			template<typename T, class CTX>
			struct parser<std::vector<T>, CTX> {
				static bool parse(std::vector<T> &v, CTX& ctx) {
					auto fn = [&v](CTX& ctx, int idx) {
						v.emplace_back();
						return parser<T,CTX>::parse(v[idx],ctx);
					};
					return parse_array(
						fn,
						ctx
					);
				}
			};
		}
	}
}

