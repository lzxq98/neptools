// fuck you boost, use our own assert

#include "../assert.hpp"

#undef BOOST_ASSERT
#undef BOOST_ASSERT_MSG
#undef BOOST_ASSERT_IS_VOID

#define BOOST_ASSERT NEPTOOLS_ASSERT
#define BOOST_ASSERT_MSG NEPTOOLS_ASSERT_MSG
#ifdef NDEBUG
#define BOOST_ASSERT_IS_VOID
#endif
