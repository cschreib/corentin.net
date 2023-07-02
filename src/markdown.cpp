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

struct quote_container {
    auto operator<=>(const quote_container&) const = default;
};

struct ulist_container {
    std::size_t level = 0;

    auto operator<=>(const ulist_container&) const = default;
};

struct olist_container {
    std::size_t level = 0;

    auto operator<=>(const olist_container&) const = default;
};

using container = std::variant<quote_container, ulist_container, olist_container>;

using containers = std::vector<container>;

struct entry {
    containers    containers;
    entry_element content;
};

using document = std::vector<entry>;
} // namespace pass1

namespace pass2 {
using pass1::code_block;
using pass1::heading;
using pass1::line;

using paragraph = std::vector<line>;

struct increase_quote_level {};
struct decrease_quote_level {};

struct increase_ulist_level {};
struct decrease_ulist_level {};

struct increase_olist_level {};
struct decrease_olist_level {};

using entry = std::variant<
    heading,
    code_block,
    increase_quote_level,
    decrease_quote_level,
    increase_ulist_level,
    decrease_ulist_level,
    increase_olist_level,
    decrease_olist_level,
    paragraph>;

using document = std::vector<entry>;
} // namespace pass2
} // namespace markdown

namespace markdown_grammar {
namespace dsl = lexy::dsl;

using input       = lexy::string_input<lexy::utf8_encoding>;
using string_view = lexy::lexeme_for<input>;

constexpr auto line_sep         = dsl::ascii::newline;
constexpr auto any_character    = -dsl::ascii::control;
constexpr auto any_but_line_sep = any_character - line_sep;

struct heading_level {
    static constexpr auto rule = dsl::capture(dsl::token(dsl::while_(dsl::hash_sign)));

    static constexpr auto value = lexy::callback<std::size_t>(
        [](string_view hashes) { return hashes.end() - hashes.begin(); });
};

struct heading_name {
    static constexpr auto rule = []() { return dsl::identifier(any_but_line_sep); }();

    static constexpr auto value = lexy::as_string<markdown::string_view>;
};

struct heading {
    static constexpr auto rule = []() {
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
        constexpr auto character = any_but_line_sep - dsl::lit_c<'`'>;
        return dsl::identifier(character);
    }();

    static constexpr auto value = lexy::as_string<markdown::string_view>;
};

struct code_block {
    static constexpr auto rule = []() {
        constexpr auto end = line_sep + LEXY_LIT("```");
        return LEXY_LIT("```") >>
               (dsl::opt(dsl::peek_not(line_sep) >> dsl::p<code_language>) + line_sep +
                dsl::capture(dsl::token(dsl::while_(dsl::peek_not(end) >> any_character))) + end);
    }();

    static constexpr auto value = lexy::callback<markdown::pass1::code_block>(
        [](std::optional<markdown::string_view> lang, string_view code) {
            return markdown::pass1::code_block{
                lang, markdown::string_view(code.begin(), code.end() - code.begin())};
        });
};

struct quote_container {
    static constexpr auto rule = dsl::peek(dsl::while_(dsl::ascii::blank) + dsl::lit_c<'>'>) >>
                                 (dsl::while_(dsl::ascii::blank) + dsl::lit_c<'>'>);

    static constexpr auto value = lexy::callback<markdown::pass1::quote_container>(
        []() { return markdown::pass1::quote_container{}; });
};

struct ulist_container {
    static constexpr auto rule = []() {
        constexpr auto bullet = dsl::lit_c<'-'> | dsl::lit_c<'*'>;
        return dsl::peek(dsl::while_(dsl::ascii::blank) + bullet + dsl::ascii::blank) >>
               (dsl::capture(dsl::token(dsl::while_(dsl::ascii::blank))) + bullet +
                dsl::ascii::blank);
    }();

    static constexpr auto value =
        lexy::callback<markdown::pass1::ulist_container>([](string_view space) {
            return markdown::pass1::ulist_container{
                static_cast<std::size_t>(space.end() - space.begin())};
        });
};

struct olist_container {
    static constexpr auto rule = []() {
        constexpr auto bullet = (dsl::n_digits<9> | dsl::n_digits<8> | dsl::n_digits<7> |
                                 dsl::n_digits<6> | dsl::n_digits<5> | dsl::n_digits<4> |
                                 dsl::n_digits<3> | dsl::n_digits<2> | dsl::ascii::digit) +
                                dsl::lit_c<'.'>;
        return dsl::peek(dsl::while_(dsl::ascii::blank) + bullet + dsl::ascii::blank) >>
               (dsl::capture(dsl::token(dsl::while_(dsl::ascii::blank))) + bullet +
                dsl::ascii::blank);
    }();

    static constexpr auto value =
        lexy::callback<markdown::pass1::olist_container>([](string_view space) {
            return markdown::pass1::olist_container{
                static_cast<std::size_t>(space.end() - space.begin())};
        });
};

struct containers {
    static constexpr auto rule =
        dsl::list(dsl::p<quote_container> | dsl::p<ulist_container> | dsl::p<olist_container>);

    static constexpr auto value = lexy::as_list<markdown::pass1::containers>;
};

struct line {
    static constexpr auto rule = []() {
        constexpr auto end = line_sep | dsl::eof;
        return dsl::capture(dsl::token(dsl::while_(dsl::peek_not(end) >> any_character)));
    }();

    static constexpr auto value = lexy::callback<markdown::pass1::line>([](string_view content) {
        return markdown::pass1::line{
            markdown::string_view(content.begin(), content.end() - content.begin())};
    });
};

struct entry {
    static constexpr auto rule =
        dsl::opt(dsl::p<containers>) + dsl::token(dsl::while_(dsl::ascii::blank)) +
        (dsl::p<heading> | dsl::p<code_block> | (dsl::else_ >> dsl::p<line>));

    static constexpr auto value = lexy::callback<markdown::pass1::entry>(
        [](std::optional<markdown::pass1::containers> containers,
           markdown::pass1::entry_element             elem) {
            return markdown::pass1::entry{containers.value_or(markdown::pass1::containers{}), elem};
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
        doc.push_back({{}, markdown::pass1::line{markdown::string_view{str.data(), str.size()}}});
        return doc;
    }

    return std::move(res.value());
}

markdown::pass2::document combine(const markdown::pass1::document& doc) {
    markdown::pass2::document output;

    markdown::pass2::paragraph* current_paragraph = nullptr;

    const markdown::pass1::containers  no_container;
    const markdown::pass1::containers* last_containers = &no_container;

    const auto flush_paragraph = [&]() {
        if (!current_paragraph) {
            return;
        }

        if (current_paragraph->empty()) {
            output.pop_back();
        }

        current_paragraph = nullptr;
    };

    for (const auto& v : doc) {
        if (!v.containers.empty()) {
            // TODO: this works for quotes, but it is not the right logic for lists.

            const auto m = std::mismatch(
                last_containers->begin(), last_containers->end(), v.containers.begin(),
                v.containers.end());

            if (m.first != last_containers->end() || m.second != v.containers.end()) {
                flush_paragraph();
            }

            for (auto iter = m.first; iter != last_containers->end(); ++iter) {
                std::visit(
                    overload{
                        [&](const markdown::pass1::quote_container& q) {
                            output.push_back(markdown::pass2::decrease_quote_level{});
                        },
                        [&](const markdown::pass1::ulist_container& u) {
                            output.push_back(markdown::pass2::decrease_ulist_level{});
                        },
                        [&](const markdown::pass1::olist_container& o) {
                            output.push_back(markdown::pass2::decrease_olist_level{});
                        }},
                    *iter);
            }

            for (auto iter = m.second; iter != v.containers.end(); ++iter) {
                std::visit(
                    overload{
                        [&](const markdown::pass1::quote_container& q) {
                            output.push_back(markdown::pass2::increase_quote_level{});
                        },
                        [&](const markdown::pass1::ulist_container& u) {
                            output.push_back(markdown::pass2::increase_ulist_level{});
                        },
                        [&](const markdown::pass1::olist_container& o) {
                            output.push_back(markdown::pass2::increase_olist_level{});
                        }},
                    *iter);
            }

            last_containers = &v.containers;
        }

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
                [&](const markdown::pass2::increase_quote_level&) { str << "<blockquote>"; },
                [&](const markdown::pass2::decrease_quote_level&) { str << "</blockquote>\n"; },
                [&](const markdown::pass2::increase_ulist_level&) { str << "<ul>"; },
                [&](const markdown::pass2::decrease_ulist_level&) { str << "</ul>\n"; },
                [&](const markdown::pass2::increase_olist_level&) { str << "<ol>"; },
                [&](const markdown::pass2::decrease_olist_level&) { str << "</ol>\n"; },
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
