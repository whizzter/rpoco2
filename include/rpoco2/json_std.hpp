#pragma once

#include "json.hpp"
#include "json_std_string.hpp"
#include "json_std_vector.hpp"
#include "json_std_map.hpp"
#include "json_std_shared.hpp"

#include <any>

namespace rpoco2 {
	namespace json {

		namespace impl {

			template<class CTX>
			struct parser<std::any, CTX> {
				static bool parse(std::any& v, CTX& ctx) {

					int ty = parse_peek(ctx);
					switch (ty) {
					case '{': {
						v.emplace<std::map<std::string, std::any>>();
						//auto& ref = ;
						return parser<std::map<std::string, std::any>, CTX>::parse(
							std::any_cast<std::map<std::string, std::any>&>(v),
							ctx
							);
					}
					case '[': {
						v.emplace<std::vector<std::any>>();
						return parser<std::vector<std::any>, CTX>::parse(
							std::any_cast<std::vector<std::any>&>(v),
							ctx
							);
					}
					case 't':
						v = true;
						return parse_known(ctx, "true");
					case 'f':
						v = false;
						return parse_known(ctx, "false");
					case 'n':
						v.reset();
						return parse_known(ctx, "null");
					case '+': {
						double t;
						auto rv = parse_number(t, ctx);
						v = t;
						return rv;
					}
					case '\"': {
						v.emplace<std::string>();
						return parser<std::string, CTX>::parse(
							std::any_cast<std::string&>(v),
							ctx
							);
					}
					default:
						return false;
					}
				}
			};
		}
	}
}

