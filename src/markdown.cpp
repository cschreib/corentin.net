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

namespace pass1 {
struct heading {
    std::size_t level = 0;
    string_view name;
};

struct code_block {
    std::optional<string_view> language;
    string_view                code;
};

using line = string_view;

using entry_element = std::variant<heading, code_block, line>;

struct entry {
    std::size_t   quote_level = 0;
    entry_element content;
};

using document = std::vector<entry>;
} // namespace pass1

namespace pass2 {
using pass1::code_block;
using pass1::heading;
using pass1::line;

using paragraph = std::vector<line>;

struct increase_quote_level {
    std::size_t amount = 0u;
};

struct decrease_quote_level {
    std::size_t amount = 0u;
};

using entry =
    std::variant<heading, code_block, increase_quote_level, decrease_quote_level, paragraph>;

using document = std::vector<entry>;
} // namespace pass2
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

    static constexpr auto value = lexy::construct<markdown::pass1::heading>;
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

    static constexpr auto value = lexy::callback<markdown::pass1::code_block>(
        [](std::optional<markdown::string_view> lang, lexeme lex) {
            return markdown::pass1::code_block{
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
        return dsl::capture(dsl::token(dsl::while_(dsl::peek_not(end) >> character)));
    }();

    static constexpr auto value = lexy::callback<markdown::pass1::line>([](lexeme line_content) {
        return markdown::pass1::line{
            markdown::string_view(line_content.begin(), line_content.end() - line_content.begin())};
    });
};

struct entry {
    static constexpr auto rule =
        dsl::opt(dsl::p<quote_level>) + dsl::token(dsl::while_(dsl::ascii::blank)) +
        (dsl::p<heading> | dsl::p<code_block> | (dsl::else_ >> dsl::p<line>));

    static constexpr auto value = lexy::callback<markdown::pass1::entry>(
        [](std::optional<std::size_t> quote_level, markdown::pass1::entry_element elem) {
            return markdown::pass1::entry{quote_level.value_or(0u), elem};
        });
};

struct document {
    static constexpr auto rule = dsl::list(dsl::p<entry>, dsl::sep(line_sep)) + dsl::eof;

    static constexpr auto value = lexy::as_list<markdown::pass1::document>;
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

markdown::pass1::document lex(const std::string& md) {
    auto str = markdown_grammar::input(md);

    // For debugging
    lexy::trace_to<markdown_grammar::document>(
        std::ostream_iterator<char>(std::cout), str, lexy_ext::report_error);

    auto res = lexy::parse<markdown_grammar::document>(str, lexy_ext::report_error);
    if (!res.has_value()) {
        markdown::pass1::document doc;
        doc.push_back({0u, markdown::pass1::line{markdown::string_view{str.data(), str.size()}}});
        return doc;
    }

    return std::move(res.value());
}

markdown::pass2::document combine(const markdown::pass1::document& doc) {
    markdown::pass2::document output;

    markdown::pass2::paragraph* current_paragraph = nullptr;
    std::size_t                 last_quote_level  = 0u;

    auto flush_paragraph = [&]() {
        if (!current_paragraph) {
            return;
        }

        if (current_paragraph->empty()) {
            output.pop_back();
        }

        current_paragraph = nullptr;
    };

    for (const auto& v : doc) {
        if (v.quote_level > last_quote_level) {
            flush_paragraph();
            output.push_back(
                markdown::pass2::increase_quote_level{v.quote_level - last_quote_level});
        } else if (v.quote_level < last_quote_level) {
            flush_paragraph();
            output.push_back(
                markdown::pass2::decrease_quote_level{last_quote_level - v.quote_level});
        }

        last_quote_level = v.quote_level;

        std::visit(
            overload{
                [&](const markdown::pass1::line& l) {
                    if (!current_paragraph && l.empty()) {
                        return;
                    }

                    if (l.empty() || !current_paragraph) {
                        // New paragraph
                        output.push_back(markdown::pass2::paragraph{});
                        current_paragraph = std::get_if<markdown::pass2::paragraph>(&output.back());
                    }

                    if (!l.empty()) {
                        current_paragraph->push_back(l);
                    }
                },
                [&](const auto& e) {
                    flush_paragraph();
                    output.push_back(e);
                }},
            v.content);
    }

    flush_paragraph();

    return output;
}

markdown::pass2::document parse(const std::string& md) {
    return combine(lex(md));
}

void html_escape(std::ostream& str, std::string_view content) {
    // TODO: \n to <br/>
    // TODO: escape "<>&" and others to "&..." format
    str << content;
}

std::string to_html(const markdown::pass2::document& doc) {
    std::ostringstream str;

    std::size_t last_quote_level = 0;
    for (const auto& ev : doc) {
        std::visit(
            overload{
                [&](const markdown::pass2::heading& h) {
                    str << "<h" << h.level << ">";
                    html_escape(str, to_string(h.name));
                    str << "</h" << h.level << ">\n";
                },
                [&](const markdown::pass2::code_block& cb) {
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
                [&](const markdown::pass2::increase_quote_level& q) {
                    for (std::size_t i = 0u; i < q.amount; ++i) {
                        str << "<blockquote>";
                    }
                },
                [&](const markdown::pass2::decrease_quote_level& q) {
                    for (std::size_t i = 0u; i < q.amount; ++i) {
                        str << "</blockquote>";
                    }
                    str << "\n";
                },
                [&](const markdown::pass2::paragraph& p) {
                    str << "<p>";

                    for (std::size_t i = 0; i < p.size(); ++i) {
                        if (i != 0) {
                            str << "<br/>\n";
                        }
                        html_escape(str, to_string(p[i]));
                    }

                    str << "</p>";
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
