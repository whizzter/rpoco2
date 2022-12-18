#pragma once

#include <ctype.h>
#include <string.h>
#include <math.h>

#include "rpoco.hpp"

namespace rpoco2 {
	namespace json {


		namespace impl {
			template<typename CTX>
			bool parse_ignore(CTX& ctx); // parse_ignore ignores everything but is a good template for parsing anything


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
					c = ctx.peek();
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
							//printf("Parsing FLT:'%c'\n", c);
							ctx.ignore(); // ok num, use it
							tm *= (DST)0.1;
							ov = ov + tm * (DST)(c - '0');
							c = ctx.peek();
						}
					}
					if (c == 'e' || c == 'E') {
						ctx.get(); // clear off eE
						int eSgn = 1;
						c = ctx.peek();
						if (c=='+') {
							ctx.ignore();
							c = ctx.peek();
						} else if (c=='-') {
							eSgn = -1;
							ctx.ignore();
							c = ctx.peek();
						}
						// clean of initial 0s
						bool isValid = false;
						while(c=='0') {
							isValid = true;
							ctx.ignore();
							c = ctx.peek();
						}
						// now we get to the digits..
						int exp=0;
						if (!isValid && (c < '0' || '9' < c))
							return false;
						while('0'<=c && c<='9') {
							exp = exp*10 + (c-'0');
							ctx.ignore();
							c = ctx.peek();
						}
						dst = ov * mul * (DST)pow(10,eSgn*exp);
					} else {
						dst = ov * mul;
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
						c = ctx.get();
						switch(c) {
						case '\"' :
						case '\\' :
						case '/' :
							if (!dst.put(c))
								return false;
							continue;
						case 'b' :
							if (!dst.put('\b'))
								return false;
							continue;
						case 'f' :
							if (!dst.put('\f'))
								return false;
							continue;
						case 'n' :
							if (!dst.put('\n'))
								return false;
							continue;
						case 'r' :
							if (!dst.put('\r'))
								return false;
							continue;
						case 't' :
							if (!dst.put('\t'))
								return false;
							continue;
						default:
							printf("Parse nonimpl-err with %c\n",c);
							return false;
						}
						// TODO
						//abort();
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

					auto keyFn = [&sw](CTX& ctx) {
						// this keyparser re-uses the buffer and asks the parse_string function to fill it in.
						sw.reset();
						return parse_string(sw, ctx);
					};

					auto valFn = [&sw, &ati, &t](CTX& ctx) {
						// after the keyparser has completed we can issue the appropriate memberparser (depending on our datatype!)
						bool ok = true;
						// we create a locator that will execute the type-specific parser.
						auto symloc = ati->locator(&t, [&ok, &ctx](auto& name, auto& dv) {
							// dispatch the type-specific parser
							ok = parser<std::remove_reference_t<decltype(dv)>, CTX>::parse(dv, ctx);
							});
						if (!symloc(sw.dest, sw.index)) {
							// UNKNOWN/Freewheel PARSING NOT YET IMPL TODO
							//printf("ERR, NEED NULL PARSER\n");
							//abort();
							//return false;
							return parse_ignore(ctx);
						}
						return ok;
					};

					// now parse the object
					return parse_object(
						keyFn, valFn, ctx);
				}
			};

			template<class CTX>
			int parse_peek(CTX& ctx) {
				if (!skip_space(ctx))
					return 0;
				char c = ctx.peek();
				switch(c) {
				case '[' : case '{' : case '\"':
				case 'n' : case 't' : case 'f' :
					return c;
				case '-' : case '+' :
				case '0' : case '1' : case '2' : case '3' : case '4':
				case '5' : case '6' : case '7' : case '8': case '9':
					return '+';
				default:
					//printf("Bad peek:%c\n",c);
					return 0;
				}
			}

			template<typename MP,class CTX>
			bool parse_array(MP& mp,CTX& ctx) {
				if (!skip_space(ctx))
					return false;
				char c=ctx.get();
				if (c!='[')
					return false;
				if (!skip_space(ctx)) return false;
				c=ctx.peek();
				int idx=0;
				while(c!=']') {
					if (!mp(ctx,idx++))
						return false;
					if (!skip_space(ctx)) return false;
					c=ctx.peek();
					if (c==',') {
						ctx.ignore();
						skip_space(ctx); // don't allow trailing comma so we won't re-load C for the next round and thus ] will be ignored and tried to be parsed!
						continue;
					} else if (c!=']') {
						return false;
					} // else would have a ] and that will break after the while
				}
				ctx.ignore(); // eat ]
				if (!skip_space(ctx)) return false;
				return true;
			}

			template<typename T,size_t L, class CTX>
			struct parser<T[L], CTX> {
				static bool parse(T(&t)[L], CTX& ctx) {
					return parse_array([&t](CTX& ctx,int idx){
						return parser<T,CTX>::parse(t[idx],ctx);
					},ctx);
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
			struct parser<double, CTX> {
				static bool parse(double& t, CTX& ctx) {
					return parse_number<double, CTX>(t, ctx);
				}
			};

			template<class CTX>
			struct parser<int, CTX> {
				static bool parse(int& t, CTX& ctx) {
					return parse_number<int, CTX>(t, ctx);
				}
			};


			template<typename CTX>
			bool parse_known(CTX& ctx,const char *tok) {
				char rc;
				while(rc=*(tok++)) {
					char c=ctx.get();
					if (rc!=c)
						return false;
				}
				return true;
			}

			template<class CTX>
			struct parser<bool, CTX> {
				static bool parse(bool& v, CTX& ctx) {
					auto pv = parse_peek(ctx);
					if (pv == 't') {
						v = true;
						return parse_known(ctx, "true");
					} else if (pv == 'f') {
						v = false;
						return parse_known(ctx, "false");
					} else return false;
				}
			};

			template<typename CTX>
			bool parse_ignore(CTX& ctx) {
				int ty = parse_peek(ctx);
				switch(ty) {
				case '{': {
					auto ignFn = [](CTX& ctx) {return parse_ignore(ctx); };
					return parse_object(ignFn,ignFn,ctx);
				}
				case '[': {
					auto ignFn = [](CTX& ctx, int idx) {return parse_ignore(ctx); };
					return parse_array(ignFn, ctx);
				}
				case 't' :
					return parse_known(ctx,"true");
				case 'f' :
					return parse_known(ctx,"false");
				case 'n' :
					return parse_known(ctx,"null");
				case '+' : {
						double t;
						return parse_number(t,ctx);
					}
				case '\"': {
						struct {
							void reset() {}
							bool put(int c) { return true; }
							void term() {}
						} nsw;
						return parse_string(nsw,ctx);
					}
				default:
					return false;
				}
			}


		}

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

	}
}
