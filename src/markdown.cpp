#include <iostream>
#include <iterator>
#include <lexy/action/parse.hpp> // lexy::parse
#include <lexy/action/trace.hpp> // lexy::trace_to
#include <lexy/callback.hpp> // value callbacks
#include <lexy/dsl.hpp> // lexy::dsl::*
#include <lexy/input/string_input.hpp> // lexy::string_input
#include <lexy_ext/report_error.hpp> // lexy_ext::report_error
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {
namespace markdown {
using string_view = std::u8string_view;

struct heading {
    std::size_t level = 0;
    string_view name;
};

struct code_block {
    std::optional<string_view> language;
    string_view                code;
};

struct line {
    std::size_t quote_level = 0;
    string_view content;
};

using paragraph = std::vector<line>;

using entry = std::variant<heading, code_block, paragraph>;

using document = std::vector<entry>;
} // namespace markdown

namespace markdown_grammar {
namespace dsl = lexy::dsl;
using input   = lexy::string_input<lexy::utf8_encoding>;

using lexeme = lexy::lexeme_for<input>;

constexpr auto line_sep = dsl::ascii::newline;

struct heading_level {
    static constexpr auto rule = dsl::capture(dsl::token(dsl::while_(dsl::hash_sign)));

    static constexpr auto value =
        lexy::callback<std::size_t>([](lexeme lex) { return lex.end() - lex.begin(); });
};

struct heading_name {
    static constexpr auto rule = []() {
        constexpr auto character = dsl::code_point - line_sep;
        return dsl::identifier(character);
    }();

    static constexpr auto value = lexy::as_string<markdown::string_view>;
};

struct heading {
    static constexpr auto rule = []() {
        constexpr auto character = dsl::code_point - line_sep;
        return dsl::peek(
                   (dsl::token(dsl::times<6>(dsl::hash_sign)) |
                    dsl::token(dsl::times<5>(dsl::hash_sign)) |
                    dsl::token(dsl::times<4>(dsl::hash_sign)) |
                    dsl::token(dsl::times<3>(dsl::hash_sign)) |
                    dsl::token(dsl::times<2>(dsl::hash_sign)) | dsl::hash_sign) +
                   dsl::ascii::blank) >>
               (dsl::p<heading_level> + dsl::token(dsl::while_one(dsl::ascii::blank)) +
                dsl::p<heading_name>);
    }();

    static constexpr auto value = lexy::construct<markdown::heading>;
};

struct code_language {
    static constexpr auto rule = []() {
        constexpr auto character = dsl::code_point - dsl::lit_c<'`'> - line_sep;
        return dsl::identifier(character);
    }();

    static constexpr auto value = lexy::as_string<markdown::string_view>;
};

struct code_block {
    static constexpr auto rule = []() {
        constexpr auto end       = line_sep + LEXY_LIT("```");
        constexpr auto character = dsl::code_point;
        return LEXY_LIT("```") >>
               (dsl::opt(dsl::peek_not(line_sep) >> dsl::p<code_language>) + line_sep +
                dsl::capture(dsl::token(dsl::while_(dsl::peek_not(end) >> character))) + end);
    }();

    static constexpr auto value = lexy::callback<markdown::code_block>(
        [](std::optional<markdown::string_view> lang, lexeme lex) {
            return markdown::code_block{
                lang, markdown::string_view(lex.begin(), lex.end() - lex.begin())};
        });
};

struct quote_level {
    static constexpr auto rule = []() {
        return dsl::peek(dsl::lit_c<'>'>) >> dsl::capture(dsl::token(dsl::while_(dsl::lit_c<'>'>)));
    }();

    static constexpr auto value =
        lexy::callback<std::size_t>([](lexeme lex) { return lex.end() - lex.begin(); });
};

struct line {
    static constexpr auto rule = []() {
        constexpr auto character = dsl::code_point;
        constexpr auto end       = line_sep | dsl::eof;
        return dsl::peek_not(end) >>
               (dsl::opt(dsl::p<quote_level>) + dsl::token(dsl::while_(dsl::ascii::blank)) +
                dsl::capture(dsl::token(dsl::while_(dsl::peek_not(end) >> character))));
    }();

    static constexpr auto value = lexy::callback<markdown::line>(
        [](std::optional<std::size_t> quote_level, lexeme line_content) {
            return markdown::line{
                quote_level.value_or(0u),
                markdown::string_view(
                    line_content.begin(), line_content.end() - line_content.begin())};
        });
};

struct paragraph {
    static constexpr auto rule = []() {
        constexpr auto character = dsl::code_point;
        constexpr auto end =
            dsl::token(dsl::times<2>(line_sep)) | dsl::token(line_sep + dsl::eof) | dsl::eof;
        return dsl::peek_not(end) >> dsl::list(dsl::p<line>, dsl::trailing_sep(line_sep));
    }();

    static constexpr auto value = lexy::as_list<markdown::paragraph>;
};

struct entry {
    static constexpr auto rule = dsl::p<heading> | dsl::p<code_block> | dsl::p<paragraph>;

    static constexpr auto value = lexy::construct<markdown::entry>;
};

struct document {
    static constexpr auto rule = dsl::list(line_sep | dsl::p<entry>) + dsl::eof;

    static constexpr auto value = lexy::as_list<markdown::document>;
};
} // namespace markdown_grammar

template<typename... Args>
struct overload : Args... {
    using Args::operator()...;
};

template<typename... Args>
overload(Args...) -> overload<Args...>;

std::string_view to_string(std::u8string_view sv) {
    // Assumes output expects UTF-8 encoded string.
    return {reinterpret_cast<const char*>(sv.begin()), sv.length()};
}

markdown::document parse(const std::string& md) {
    auto str = markdown_grammar::input(md);
    lexy::trace_to<markdown_grammar::document>(
        std::ostream_iterator<char>(std::cout), str, lexy_ext::report_error);
    auto res = lexy::parse<markdown_grammar::document>(str, lexy_ext::report_error);
    if (!res.has_value()) {
        markdown::document doc;
        doc.push_back(markdown::paragraph{
            {markdown::line{0, markdown::string_view{str.data(), str.size()}}}});
        return doc;
    }

    return std::move(res.value());
}

void html_escape(std::ostream& str, std::string_view content) {
    // TODO: \n to <br/>
    // TODO: escape "<>&" and others to "&..." format
    str << content;
}

std::string to_html(const markdown::document& doc) {
    std::ostringstream str;

    for (const auto& ev : doc) {
        std::visit(
            overload{
                [&](const markdown::heading& h) {
                    str << "<h" << h.level << ">";
                    html_escape(str, to_string(h.name));
                    str << "</h" << h.level << ">\n";
                },
                [&](const markdown::code_block& cb) {
                    str << "<pre>";
                    if (cb.language.has_value()) {
                        str << "<code class=\"";
                        html_escape(str, to_string(cb.language.value()));
                        str << "\">";
                    } else {
                        str << "<code>";
                    }
                    html_escape(str, to_string(cb.code));
                    str << "</code></pre>\n";
                },
                [&](const markdown::paragraph& p) {
                    if (p.empty()) {
                        return;
                    }

                    std::size_t last_quote_level = 0;
                    while (p[0].quote_level > last_quote_level) {
                        str << "<blockquote>";
                        ++last_quote_level;
                    }
                    str << "<p>";

                    for (std::size_t i = 0; i < p.size(); ++i) {
                        if (i != 0) {
                            str << "\n";
                        }
                        if (p[i].quote_level > last_quote_level) {
                            str << "</p>\n";
                            while (p[i].quote_level > last_quote_level) {
                                str << "<blockquote>";
                                ++last_quote_level;
                            }
                            str << "<p>";
                        }
                        if (p[i].quote_level < last_quote_level) {
                            str << "</p>";
                            while (p[i].quote_level < last_quote_level) {
                                str << "</blockquote>";
                                --last_quote_level;
                            }
                            str << "\n<p>";
                        }
                        html_escape(str, to_string(p[i].content));
                    }

                    str << "</p>";
                    while (last_quote_level > 0) {
                        str << "</blockquote>";
                        --last_quote_level;
                    }
                    str << "\n";
                }},
            ev);
    }

    return str.str();
}
} // namespace

std::string markdown2html(const std::string& md) {
    return to_html(parse(md));
}
