#ifndef __INCLUDED_RPOCO2_HPP__
#define __INCLUDED_RPOCO2_HPP__

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>
#include <tuple>

#define RPOCO2(...)  auto rpoco2_type_info_get() {\
	using rpoco2::impl2::opt; \
	using T=decltype(*this); \
	static constexpr auto names = rpoco2::impl2::parse<rpoco2::impl2::parse<-1>("" #__VA_ARGS__)>("" #__VA_ARGS__); \
	static constexpr auto snames = rpoco2::impl2::sort(names); \
	static constexpr auto ti = rpoco2::impl2::produce(\
		names, \
		snames, \
		rpoco2::impl2::optize(std::tuple{}, __VA_ARGS__)); \
	return &ti; \
}

namespace rpoco2 {

	namespace json {
		static constexpr const class ignore {
			constexpr ignore() {}
		public:
			static ignore dummy;
		} ignore=ignore::dummy;
	};

	struct type_info {
		virtual int member_count() const=0;
		virtual const std::string_view& member_name(int idx) const=0;
	};

	namespace impl2 {
		
		using opt = std::tuple;

		template<std::size_t OFFSET,typename... T,std::size_t... I>
		constexpr auto remove_tuple_head_detail(const std::tuple<T...>& t, std::index_sequence<I...>) {
			return std::make_tuple(std::get<OFFSET + I>(t)...);
		}

		template<std::size_t OFFSET=1,typename ...T>
		constexpr auto remove_tuple_head(const std::tuple<T...>& t) {
			return remove_tuple_head_detail<OFFSET>(t, std::make_index_sequence<sizeof...(T) - OFFSET>());
		}

		template<class ... INIT>
		constexpr auto optize(const std::tuple<INIT...> init) {
			return init;
		}

		template<class ... INIT, class B,class MT,class ... O, class ...R>
		constexpr auto optize(const std::tuple<INIT...> init, const std::tuple<MT B::*,O...> aopt, R&&... r) {
			return optize(
				std::tuple_cat(init, std::make_tuple(std::make_tuple(std::get<0>(aopt),remove_tuple_head(aopt)))),
				std::forward<R>(r)...
			);
		}
		template<class ... INIT, class B, class MT, class ...R>
		constexpr auto optize(const std::tuple<INIT...> init, MT B::* nopt, R&&... r) {
			return optize(
				std::tuple_cat(init, std::make_tuple(std::make_tuple(nopt,std::make_tuple()))),
				std::forward<R>(r)...
			);
		}

		enum class member_location_result {
			not_done=0,
			found,
			not_found
		};

		class member_locator {
		public:
			virtual member_location_result feed(char c)=0;
			virtual bool operator()() = 0;
			virtual bool operator()(const char* n, int len = -1) = 0;
		};

		template<class BASE, class NAMES, class TNAME>
		class t_ti;

		// NAMES is of type std::array<NAMEIDX,SZ>
		// TNAME is of member ptr types
		template<class BASE,class NAMES, typename ...R, typename ...O>
		class t_ti<BASE,NAMES, std::tuple<std::tuple<R BASE::*, O>...> > : public type_info {
			using TNAME = std::tuple<std::tuple<R BASE::*, O>...>;
		private:
			NAMES names;
			NAMES snames;
			TNAME members;

			template<class F, std::size_t ... I>
			inline void foreach_impl(BASE* bp, const F& fn, std::index_sequence<I...> idxs) const {
				(fn(names[I].name, bp->*(std::get<0>(std::get<I>(members)))),...);
			}
			template<class F, std::size_t ... I>
			inline void member_apply_impl(BASE* bp, int idx, const F& fn, std::index_sequence<I...> idxs) const {
				using fnptr = void(*)(const t_ti*,BASE * bp,const F& fn);
				static fnptr ptrs[sizeof...(R)] = { ([](const t_ti * px,BASE* bp,const F& fn) {
					auto mp = std::get<0>(std::get<I>(px->members));
					fn(px->names[I].name,bp->*mp);
				})... };
				ptrs[idx](this,bp,fn);
			}
		public:

			virtual int member_count() const {
				return names.size();
			}
			virtual const std::string_view& member_name(int idx) const {
				return names[idx].name;
			}

			template<class F>
			auto locator(BASE *bp,const F& fn) const {
				class loc_impl : public member_locator {
					const t_ti* pti;
					const F& fn;
					BASE* bp;
					int idx;
					int low, high;
					inline void feedone(char c) {
						while (low <= high) {
							auto& ln = pti->snames[low].name;
							auto& hn = pti->snames[high].name;
							if (idx >= ln.size() || ln[idx] < c) {
								low++;
								continue;
							} else if (idx >= hn.size() || hn[idx] > c) {
								high--;
								continue;
							} else break;
						}
						idx++;
					}
				public:
					virtual member_location_result feed(char c) {
						feedone(c);
						if (low > high) {
							return member_location_result::not_found;
						} else if (pti->snames[low].name.size() == idx) {
							return member_location_result::found;
						} else return member_location_result::not_done;
					};

					virtual bool operator()() {
						if (low > high) {
							return false;
						} else if (pti->snames[low].name.size() == idx) {
							pti->member_apply(bp, pti->snames[low].idx, fn);
							return true;
						} else {
							return false;
						}
					}
					virtual bool operator()(const char* n, int len = -1) {
						if (len == -1)
							len = strlen(n);
						for (int i = 0; i < len; i++)
							feedone(n[i]);
						return operator()();
					}
					loc_impl(const t_ti* ipti,BASE *ibp, const F& ifn) : pti(ipti),bp(ibp) , fn(ifn),idx(0), low(0), high(sizeof...(R)-1) {}
				};

				return loc_impl{ this,bp,fn };
			}

			template<class F>
			void member_apply(BASE* bp, int idx, const F& fn) const {
				member_apply_impl(bp, idx, fn, std::make_index_sequence<sizeof...(R)>());
			}

			template<class F>
			void foreach(BASE* bp, const F& fn) const {
				foreach_impl(bp, fn, std::make_index_sequence<sizeof...(R)>());
			}

			constexpr t_ti(NAMES i_names, NAMES i_snames, TNAME i_mem) : names(i_names), snames(i_snames), members(i_mem)
				//,members(itup)
			{}
		};


		template<typename BASE,typename NAMES,typename ...R,typename ...O>
		constexpr auto produce(NAMES names,NAMES snames,std::tuple<std::tuple<R BASE::*,O>...> r) {
			using TNAME = decltype(r); // tup<R BASE:: *...>;
			
			return t_ti<BASE,NAMES,TNAME>{ names,snames,r };
		}

		enum class parse_state {
			DEF=0,           // default state
			OPTTOK,          // if we're parsing a member opt{
			PREOPTNAME,      // after the opt{ part but before the member
			TNAME,           // memberptr-typename?
			OPTDATA,         // option data
			OPTSTR,
			POST_OPTMEMBER   // post-opt
		};

		constexpr bool isspace(char c) {
			switch(c) {
			case ' ': case '\t' : case '\n' : case '\r' : case '\b' :
				return true;
			}
			return false;
		}

		// keep track of name and original index (to lookup the actual sym)
		struct nameidx {
			std::string_view name;
			int idx;

			constexpr nameidx():name(),idx() {}
		};

		// just a small specialized bubblesort (no constexpr std::sort in C++ 17)
		template<int COUNT>
		constexpr auto sort(std::array<nameidx,COUNT> iv) {
			bool sorted=false;
			while(!sorted) {
				sorted=true;
				for (std::size_t i=1;i<COUNT;i++) {
					if (iv[i-1].name>iv[i].name) {
						auto tmp=iv[i];
						iv[i]=iv[i-1];
						iv[i-1]=tmp;
						sorted=false;
					}
				}
			}
			return iv;
		}

		// constexpr parser of a list of comma separated literal tokens (identifiers)
		template<int COUNT>
		constexpr auto parse(std::string_view  sv) {
			int ns=0; // namestart
			int nc=0; // name-count
			//int coco=0;
			int lsc = 0;
			int optType = 0;
			int optCount = 0;

			//std::string_view xsout[COUNT>=0?COUNT:1]; // MSVC COMPILER BUG?! without this the evaluation below will fail?
			constexpr std::size_t SOUTSIZE=COUNT>=0?COUNT:0;

			// our output array (size determined by previous invocation)
			std::array<nameidx,SOUTSIZE> sout;
			parse_state state=parse_state::DEF;

			for (int i=0;i<sv.size();i++) {
				char c=sv[i];
				switch (state) {
				case parse_state::DEF: {
					if (c == ',')
						throw std::exception("comma not expected without name");
					if (isspace(c))
						continue;
					else if (c == '&' || c == 'o') {
						state = c == '&' ? parse_state::TNAME : parse_state::OPTTOK;
						//coco=0;
						if constexpr (COUNT != -1) {
							//sout[nc].name=sv.substr(i,1);
							sout[nc].idx = nc;
						}
						optType = 0;
						nc++;
						ns = i + 1;
						continue;
					} else throw std::exception("RPOCO can only contain pointer to members OR opt's with pointer to members");
				}
				case parse_state::OPTTOK: {
					int ofs = i - (ns - 1);
					if (ofs < 3) {
						if ("opt"[ofs] != c) {
							throw std::exception("RPOCO can only contain pointer to members OR opt's with pointer to members");
						} else continue;
					}
					if (isspace(c)) {
						continue;
					} else if (c == '{') {
						state = parse_state::PREOPTNAME;
						optType = c;
						continue;
					} else {
						//static_assert(0, "Invalid char after opt");
						throw std::exception("Invalid char after opt");
					}
				}
				case parse_state::PREOPTNAME:
					if (isspace(c)) {
						continue;
					} else if (c == '&') {
						ns = i + 1;
						state = parse_state::TNAME;
						continue;
					} else throw new std::exception("opt{ must be followed by a member pointer");
				case parse_state::TNAME: {
					if (c == ':') {
						ns = i + 1;
						continue;
					} else if (isspace(c)) {
						state = optType == 0 ? parse_state::POST_OPTMEMBER : parse_state::OPTDATA;
						continue;
					} else if (c == ',') {
						state = optType == 0 ? parse_state::DEF : parse_state::OPTDATA;
						continue;
					} else {
						if constexpr (COUNT != -1) {
							sout[nc - 1].name = sv.substr(ns, 1 + i - ns);
						}
						continue;
					}
				}
				case parse_state::POST_OPTMEMBER :
					if (isspace(c)) {
						continue;
					} else if (c==',') {
						state=parse_state::DEF;
						continue;
					}
					throw std::exception("comma or spaces must be after name");
				case parse_state::OPTDATA: {
					int eOpt = optType == '{' ? '}' : ')';
					if (c == '\"') {
						state = parse_state::OPTSTR;
						lsc = 0;
						continue;
					} else if (c == optType) {
						optCount++;
						continue;
					} else if (c == eOpt) {
						if (optCount == 0) {
							state = parse_state::POST_OPTMEMBER;
							continue;
						}
						optCount--;
						continue;
					} else {
						continue;
					}
				} break;
				case parse_state::OPTSTR :
					if (lsc == '\\') {
						lsc = 0;
						continue;
					} else if (c == '\"') {
						state = parse_state::OPTDATA;
						continue;
					} else {
						lsc = c;
						continue;
					}
				default:
					throw std::exception("bad state");
				}
				//nc+=sv[i];
			}
			if constexpr (COUNT==-1) {
				return nc;
			} else {
				return sout;
			}
		}

	};
};

#endif __INCLUDED_RPOCO2_HPP__

