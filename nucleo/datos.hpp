#pragma once
#include "tipos.hpp"
#include <memory>
#include <vector>

namespace extras {
inline bool EsFinito(Vector3 p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
struct ContadorImpactos {
    uintptr_t personaje=0,servicio=0;
    int impactos=0;
    bool Actualizar(uintptr_t siguientePersonaje,uintptr_t siguienteServicio,int cantidad) {
        const bool impacto=siguientePersonaje&&siguienteServicio&&personaje==siguientePersonaje&&servicio==siguienteServicio&&cantidad>impactos;
        personaje=siguientePersonaje; servicio=siguienteServicio; impactos=cantidad;
        return impacto;
    }
};
struct EstadoBomba {
    Vector3 origen{};
    bool visible=false,plantada=false,tiempoValido=false,desactivando=false;
    int zona=0;
    float restante=0,duracion=0,desactivacionRestante=0;
};
inline bool CuentaAtrasBomba(float fin,float ahora,float duracion,float& restante) {
    if (!std::isfinite(fin)||!std::isfinite(ahora)||!std::isfinite(duracion)||ahora<0||duracion<10||duracion>120) return false;
    restante=fin-ahora;
    return restante>0&&restante<=duracion+.5f;
}
struct Instantanea {
    std::uint64_t leidoEn=0,impactoEn=0,bombaLeidaEn=0;
    EstadoBomba bomba{};
};
}
void ActualizarDatosExtras();
void ReiniciarDatosExtras();
extras::Instantanea CopiarDatosExtras();

namespace mundo {
enum class Tipo { Ninguno,Arma,HE,Flash,Humo,Molotov,Senuelo,Fuego };
struct Objeto {
    Vector3 origen{},velocidad{};
    Tipo tipo=Tipo::Ninguno;
    float restante=-1;
    int balasCargador=-1,balasReserva=-1;
    std::array<char,64> nombre{};
};
struct Cuadro { std::uint64_t leidoEn=0; std::vector<Objeto> objetos; };
void Actualizar();
void Reiniciar();
std::shared_ptr<const Cuadro> CopiarDatos();
}
