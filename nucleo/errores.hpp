#pragma once
#include <mutex>
#include <string>

namespace errores {
inline std::mutex mutex;
inline std::string ultimoError;

inline void Error(const std::string& mensaje) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex);
        ultimoError=mensaje;
    } catch (...) {}
}

inline std::string ErrorActual() {
    std::lock_guard<std::mutex> lock(mutex);
    return ultimoError;
}

inline void ErrorWindows(const char* operacion,long codigo) {
    Error(std::string(operacion)+" (código "+std::to_string(codigo)+")");
}
}
