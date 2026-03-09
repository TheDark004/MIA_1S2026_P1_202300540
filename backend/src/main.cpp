#include <iostream>
#include <string>

// crow_all.h es el servidor HTTP que vive en external/
// Maneja los POST /execute y GET /report del frontend
#define CROW_MAIN
#include "../../external/crow_all.h"

// nlohmann/json para parsear y generar JSON
#include <nlohmann/json.hpp>

#include "Analyzer/Analyzer.h"

using json = nlohmann::json;

int main() {
    
    std::cout << "   ExtreamFS — Backend C++ + Crow HTTP\n";
    std::cout << "   Puerto : 18080\n";
    std::cout << "========================================\n";

    //  Crear la app de Crow 
    // Crow es el servidor HTTP. crow::SimpleApp es la clase base.
    crow::SimpleApp app;

    //  Endpoint: POST /execute
    // El frontend envía: { "commands": "mkdisk -size=3 ..." }
    // El backend retorna: { "output": "Disco creado..." }
    // CROW_ROUTE define una ruta. El método .methods() especifica
    // que solo acepta POST (no GET).

    CROW_ROUTE(app, "/execute").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        // Parsear el JSON del body del request
        auto body = json::parse(req.body, nullptr, false);

        // Si el JSON es inválido, retornar error
        if (body.is_discarded() || !body.contains("commands")) {
            json err = {{"output", "Error: JSON inválido o falta campo 'commands'"}};
            auto res = crow::response(400, err.dump());
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        // Extraer el string de comandos
        std::string commands = body["commands"].get<std::string>();

        // Analizar el script (puede ser uno o varios comandos)
        std::string output = Analyzer::AnalyzeScript(commands);

        // Construir y retornar la respuesta JSON
        json response = {{"output", output}};

        // Configurar headers para que el frontend React pueda acceder
        auto res = crow::response(200, response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*"); // CORS para React
        return res;
    });

    // Endpoint: OPTIONS /execute 
    // Los navegadores hacen un "preflight" OPTIONS antes de POST.
    // Debemos responder 200 con los headers CORS correctos.
    CROW_ROUTE(app, "/execute").methods(crow::HTTPMethod::OPTIONS)
    ([]() {
        auto res = crow::response(200);
        res.add_header("Access-Control-Allow-Origin",  "*");
        res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        return res;
    });

    // GET /health — verificar que el backend corre
    CROW_ROUTE(app, "/health")
    ([]() {
        json r = {{"status", "ok"}, {"backend", "ExtreamFS C++"}};
        auto res = crow::response(200, r.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // Iniciar el servidor 
    // Puerto 18080 para no chocar con otros servicios comunes
    std::cout << "Servidor iniciado en http://localhost:18080\n";
    std::cout << "POST http://localhost:18080/execute\n";
    std::cout << "GET  http://localhost:18080/health\n\n";
    app.port(18080).multithreaded().run();

    return 0;
}