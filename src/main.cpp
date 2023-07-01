#include "html.hpp"
#include "utility.hpp"

#include <crow/app.h>

int main() {
    crow::SimpleApp app;

    crow::logger::setLogLevel(crow::LogLevel::Debug);

    app.exception_handler([](crow::response& res) {
        try {
            throw;
        } catch (const std::exception& e) {
            res      = serve_markdown("500.md", {{"message", e.what()}});
            res.code = crow::status::INTERNAL_SERVER_ERROR;
        }
    });

    CROW_CATCHALL_ROUTE(app)
    ([](const crow::request& req) {
        // Static content
        const auto file_url = req.url.substr(1);
        const auto url      = std::string_view(file_url);
        if (url.ends_with(".css")) {
            return serve_css(file_url);
        } else if (url.ends_with(".png")) {
            return serve_file(file_url);
        }

        // Markdown pages
        if (file_exists(file_url + ".md")) {
            return serve_markdown(file_url + ".md");
        }
        if (file_exists(file_url + "index.md")) {
            return serve_markdown(file_url + "index.md");
        }

        // Not found
        if (file_exists("404.md")) {
            auto res = serve_markdown("404.md");
            res.code = crow::status::NOT_FOUND;
            return res;
        } else {
            return crow::response(crow::status::NOT_FOUND);
        }
    });

    app.port(8001).multithreaded().run();
}
