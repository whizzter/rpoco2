#pragma once

#include "json.hpp"
#include <memory>

namespace rpoco2 {
	namespace json {
		namespace impl {
			template<typename T, class CTX>
			struct parser<std::shared_ptr<T>, CTX> {
				static bool parse(std::shared_ptr<T>& v, CTX& ctx) {

					auto peek = parse_peek(ctx);
					if (!peek)
						return false;
					if (peek == 'n') {
						return parse_known(ctx,"null");
					} else {
						v = std::make_shared<T>();
						return parser<T, CTX>::parse(*v, ctx);
					}
				}
			};
		}
	}
}


