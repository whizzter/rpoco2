#include <ctype.h>
#include <string.h>

namespace rpoco2 {
	namespace json {

		template<class T>
		bool parse(T& dest, const char* src, int len = -1) {
			struct SA {
				const char* src;
				int len;
				int idx = 0;
				inline bool eof() {
					if (len < 0) {
						return !src[idx];
					} else {
						return idx < len;
					}
				}
				inline char peek() {
					if (eof())
						return 0;
					return src[idx];
				}
				inline char get() {
					if (eof())
						return 0;
					return src[idx++];
				}
				inline void ignore() {
					idx++;
				}
			}sa{ src,len };

			return impl::parser<T, SA>::parse(dest, sa);
		}

		namespace impl {
			template<class CTX>
			inline bool skip_space(CTX& ctx) {
				while (!ctx.eof() && isspace(ctx.peek())) {
					ctx.ignore();
				}
				return !ctx.eof();
			}

			template<class DST, class CTX>
			inline bool parse_number(DST& dst, CTX& ctx) {
				if (!skip_space(ctx))
					return false;
				DST mul;
				bool neg = ctx.peek() == '-';
				if (neg) {
					mul = -1;
					ctx.ignore();
				} else {
					mul = 1;
				}
				DST ov = 0;
				auto c = ctx.get();
				if (c == '0') {
				} else if ('1' <= c || c <= '9') {
					while (true) {
						ov = ov * 10 + (c - '0');
						c = ctx.peek();
						if (c < '0' || '9' < c)
							break;
						ctx.ignore();
					}
				} else return false;
				if constexpr (std::is_integral_v<DST>) {
					// TODO: parse zero fractions (and exponent?)
					dst = ov * mul;
					return true;
				} else {
					DST tm = 1;
					if (c == '.') {
						ctx.get(); // clear off dot
						c = ctx.peek();
						if (c < '0' || '9' < c)
							return false;
						while ('0' <= c && c <= '9') {
							printf("Parsing FLT:'%c'\n", c);
							ctx.ignore(); // ok num, use it
							tm *= (DST)0.1;
							ov = ov + tm * (DST)(c - '0');
							c = ctx.peek();
						}
					}
					if (c == 'e' || c == 'E') {
						ctx.get();
						abort(); // TODO
						dst = ov * mul; // TODO: exponent
					} else {
						dst = ov * mul; // TODO: exponent
					}
					return true;
				}
				//std::is_integral<DST>
			}

			template<class DST, class CTX>
			inline bool parse_string(DST& dst, CTX& ctx) {
				if (!skip_space(ctx))
					return false;
				if (ctx.get() != '\"')
					return false;
				while (ctx.peek() != '\"') {
					if (ctx.eof())
						return false;
					auto c = ctx.get();
					if (c == '\\') {
						// TODO
						abort();
					} else {
						if (!dst.put(c))
							return false;
					}
				}
				dst.term();
				if (ctx.get() != '\"')
					return false;
				return true;
			}

			template<class T>
			struct string_writer;

			template<class CT, size_t L>
			struct string_writer<CT[L]> {
				CT* dest;
				size_t index = 0;

				void reset() {
					index = 0;
				}
				bool put(CT v) {
					if (index >= L - 1)
						return false;
					dest[index++] = v;
					return true;
				}
				void term() {
					dest[index] = 0;
				}
			};

			//template<class CT>
			//struct string_writer<std::basic_string<CT>> {
			//	std::basic_string<CT> dest;
			//};

			// Parseobject expects a keyparser, memberparser and context as inputs
			template<class KP, class MP, class CTX>
			bool parse_object(KP& kp, MP& mp, CTX& ctx) {
				// ignore possible spaces before the object
				if (!skip_space(ctx))
					return false;
				// read an object start token or fail
				if (ctx.get() != '{')
					return false;
				// ignore pre-key/end spaces
				if (!skip_space(ctx))
					return false;
				// empty object?
				if (ctx.peek() == '}') {
					ctx.ignore();
					return true;
				}
				// then read until we've finished
				while(true) {
					// parse the key
					if (!kp(ctx))
						return false;
					// ignore pre-:-separator spaces
					if (!skip_space(ctx))
						return false;
					// expect or fail if : is missing
					if (ctx.get() != ':')
						return false;
					// parse the member
					if (!mp(ctx))
						return false;
					// ignore pre-end/end spaces
					if (!skip_space(ctx))
						return false;
					auto c = ctx.peek();
					if (c == ',') {
						// more members, ignore the comma
						ctx.ignore();
						continue;
					} else if (c == '}') {
						// end of object, eat the } and return success!
						ctx.ignore();
						return true;
					} else return false; // unknown token encountered
				}
			}

			template<class T, class CTX>
			struct parser {
				static bool parse(T& t, CTX& ctx) {
					// get type-info
					const auto* ati = t.rpoco2_type_info_get();
					// make a fixed 100 byte buffer for keys
					char symname[100];
					string_writer<decltype(symname)> sw{ symname };

					// now parse the object
					return parse_object(
						[&sw](CTX& ctx) {
							// this keyparser re-uses the buffer and asks the parse_string function to fill it in.
							sw.reset();
							return parse_string(sw, ctx);
						}, [&sw, &ati, &t](CTX& ctx) {
							// after the keyparser has completed we can issue the appropriate memberparser (depending on our datatype!)
							bool ok = true;
							// we create a locator that will execute the type-specific parser.
							auto symloc = ati->locator(&t, [&ok, &ctx](auto& name, auto& dv) {
								// dispatch the type-specific parser
								ok = parser<std::remove_reference_t<decltype(dv)>, CTX>::parse(dv, ctx);
							});
							if (!symloc(sw.dest, sw.index)) {
								// UNKNOWN/Freewheel PARSING NOT YET IMPL TODO
								abort();
								return false;
							}
							return ok;
						}, ctx);
				}
			};

			template<size_t L, class CTX>
			struct parser<char[L], CTX> {
				static bool parse(char(&t)[L], CTX& ctx) {
					string_writer<char[L]> sw{ t };
					return parse_string(sw, ctx);
				}
			};

			template<class CTX>
			struct parser<float, CTX> {
				static bool parse(float& t, CTX& ctx) {
					return parse_number<float, CTX>(t, ctx);
				}
			};

			template<class CTX>
			struct parser<int, CTX> {
				static bool parse(int& t, CTX& ctx) {
					return parse_number<int, CTX>(t, ctx);
				}
			};

		}
	}
}
