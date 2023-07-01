#include "html.hpp"

#include "markdown.hpp"
#include "utility.hpp"

#include <crow/mustache.h>
#include <fstream>

void set_html_header(crow::response& response) {
    response.add_header("Access-Control-Allow-Origin", "*");
    response.add_header(
        "Content-Security-Policy", "script-src 'self' 'unsafe-eval' 'unsafe-inline'; "
                                   "default-src 'self' 'unsafe-inline' *.corentin.net");
    response.add_header("Strict-Transport-Security", "max-age=63072000");
    response.add_header("X-Frame-Options", "DENY");
    response.add_header("X-Content-Type-Options", "nosniff");
}

crow::response serve_file(const std::string& filename) {
    if (filename.empty()) {
        throw std::runtime_error("cannot serve './' as file");
    }

    std::string adjusted_filename = filename;
    if (adjusted_filename[0] == '/') {
        adjusted_filename = adjusted_filename.substr(1);
    }

    crow::response response;
    response.set_static_file_info(filename);
    if (response.get_header_value("Content-Type") == "text/html") {
        // HTML page
        set_html_header(response);
    } else {
        // Image/script/etc
        response.add_header("Access-Control-Allow-Origin", "*");
    }

    return response;
}

crow::response serve_css(const std::string& filename) {
    auto response = serve_file(filename);
    // response.body = sass2css(response.body);
    return response;
}

crow::response serve_markdown(const std::string& filename, const crow::json::wvalue& ctx) {
    crow::response response;
    response.code = crow::status::OK;
    response.add_header("Content-Type", "text/html");
    set_html_header(response);

    response.body += read_neighbor_file(filename, "header.html");
    response.body += markdown2html(read_file(filename));
    response.body += read_neighbor_file(filename, "footer.html");

    response.body = crow::mustache::compile(response.body).render_string(ctx);

    return response;
}
