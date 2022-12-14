#
# Documentation
#

find_package(Doxygen REQUIRED dot)

FetchContent_Declare(
	doxygen_awesome_css
	GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
	GIT_TAG        a3c119b4797be2039761ec1fa0731f038e3026f6
)

FetchContent_MakeAvailable(doxygen_awesome_css)

set(DOXYGEN_PROJECT_NAME "Ham")
set(DOXYGEN_PROJECT_BRIEF "The Ham devlopment environment.")
set(DOXYGEN_PROJECT_LOGO "${PROJECT_SOURCE_DIR}/engine/editor/images/logo.png")

set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
set(DOXYGEN_NUM_PROC_THREADS 0)

set(DOXYGEN_STRIP_FROM_PATH "${HAM_ALL_INCLUDE_DIRS}")

set(DOXYGEN_EXTENSION_MAPPING "h=C++")
set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
set(DOXYGEN_EXTRACT_STATIC YES)

set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
set(DOXYGEN_CLANG_DATABASE_PATH "${PROJECT_BINARY_DIR}/compile_commands.json")
set(DOXYGEN_CLANG_OPTIONS "-std=gnu++20")

set(DOXYGEN_ENABLE_PREPROCESSING YES)
set(DOXYGEN_MACRO_EXPANSION YES)
set(DOXYGEN_EXPAND_ONLY_PREDEF NO)
set(
	DOXYGEN_PREDEFINED
	"__attribute__()="
	"HAM_C_API_BEGIN="
	"HAM_C_API_END="
	"ham_used="
	"ham_api="
	"ham_engine_api="
	#"ham_nothrow="
	"ham_nonnull_args(...)="
	"ham_derive(x)=x HAM_SUPER_NAME\;"
	#"ham_constexpr=constexpr"
	"__cplusplus=1"
)

set(DOXYGEN_INCLUDE_PATH "${HAM_SOURCE_INCLUDE_DIR};${HAM_BINARY_INCLUDE_DIR}")

set(DOXYGEN_INLINE_SIMPLE_STRUCTS YES)

set(DOXYGEN_HAVE_DOT YES)
set(DOXYGEN_DOT_IMAGE_FORMAT svg)
set(DOXYGEN_DOT_TRANSPARENT YES)

set(DOXYGEN_IMAGE_PATH "${HAM_ENGINE_SOURCE_DIR}/editor/images")

set(DOXYGEN_HTML_COLORSTYLE AUTO_DARK)
set(DOXYGEN_GENERATE_TREEVIEW YES)
set(DOXYGEN_DISABLE_INDEX NO)
set(DOXYGEN_FULL_SIDEBAR NO)
set(DOXYGEN_HTML_HEADER "header.html")
set(
	DOXYGEN_HTML_EXTRA_FILES
	"${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome-darkmode-toggle.js"
	"${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome-fragment-copy-button.js"
	"${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome-paragraph-link.js"
	"${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome-interactive-toc.js"
)
set(
	DOXYGEN_HTML_EXTRA_STYLESHEET
	"${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome.css"
	"${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome-sidebar-only.css"
	"${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome-sidebar-only-darkmode-toggle.css"
)

set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${CMAKE_CURRENT_LIST_DIR}/mainpage.md)

doxygen_add_docs(
	ham-runtime-docs
	${CMAKE_CURRENT_LIST_DIR}/mainpage.md
	${CMAKE_CURRENT_LIST_DIR}/runtime.md
	${CMAKE_CURRENT_LIST_DIR}/engine.md
	${HAM_ALL_HEADERS}
	USE_STAMP_FILE
)
