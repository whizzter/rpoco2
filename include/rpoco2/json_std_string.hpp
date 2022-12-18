#pragma once

#include "json.hpp"
#include <string>

namespace rpoco2 {
	namespace json {
		namespace impl {

			template<class CTX>
			struct parser<std::string, CTX> {
				static bool parse(std::string &s, CTX& ctx) {
					struct {
						std::string * ptr;
						void reset() {
							ptr->clear();
						}
						bool put(char c) {
							ptr->push_back(c);
							return true;
						}
						void term() {}
					}ssw;
					ssw.ptr = &s;
					return parse_string(ssw, ctx);
				}
			};

		}
	}
}

