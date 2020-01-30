#include <ctype.h>
#include <string.h>

namespace rpoco2 {
	namespace json {

		template<class T>
		bool parse(T& dest,const char * src,int len=-1) {
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
					printf("Ignoring(%c)\n",src[idx]);
					idx++;
				}
			}sa{ src,len };

			return impl::parser<T,SA>::parse(dest, sa);
		}

		namespace impl {
			template<class CTX>
			inline bool skip_space(CTX& ctx) {
				while (!ctx.eof() && isspace(ctx.peek())) {
					ctx.ignore();
				}
				return !ctx.eof();
			}

			template<class DST,class CTX>
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
				DST ov=0;
				auto c = ctx.get();
				if (c == '0') {
				} else if ('1'<=c || c <= '9') {
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
						while ('0'<=c && c<='9') {
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

			template<class DST,class CTX>
			inline bool parse_string(DST& dst,CTX& ctx) {
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

			template<class CT,size_t L>
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


			template<class T,class CTX>
			struct parser {
				static bool parse(T& t,CTX& ctx) {
					const auto* ati = t.rpoco2_type_info_get();

					if (!skip_space(ctx))
						return false;
					if (ctx.get()!='{')
						return false;
					if (!skip_space(ctx))
						return false;
					while (ctx.peek() != '}') {
						// TODO: generalize to passing symstringwriter..
						char symname[100];
						string_writer<decltype(symname)> sw{ symname };
						if (!parse_string(sw,ctx))
							return false;
						if (!skip_space(ctx))
							return false;
						if (ctx.get() != ':')
							return false;
						bool ok = true;
						auto symloc = ati->locator(&t, [&ok,&ctx](auto& name, auto& dv) {
							ok = parser<std::remove_reference_t<decltype(dv)>, CTX>::parse(dv, ctx);
						});
						if (!ok)
							return false;
						if (!symloc(symname, sw.index)) {
							// UNKNOWN PARSING NOT YET IMPL TODO
							return false;
						}
						if (!skip_space(ctx))
							return false;
						auto c = ctx.peek();
						if (c == ',') {
							ctx.ignore();
							if (!skip_space(ctx))
								return false;
							continue;
						} else if (c == '}') {
							ctx.ignore();
							return true;
						} else return false;
					}
					return false;
				}
			};

			template<size_t L,class CTX>
			struct parser<char[L],CTX> {
				static bool parse(char (&t)[L], CTX& ctx) {
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
