#pragma once
#include <Windows.h>
#include <cstdint>
#include "direcciones.hpp"
#include "tipos.hpp"
#include <cstddef>
#include <string>
#include <vector>
#include <type_traits>
#include <array>

namespace lecturas {
enum Opcion : std::uint32_t {
    Jugadores=1u<<0,Nombre=1u<<1,Arma=1u<<2,Apuntado=1u<<4,
    Portador=1u<<5,Dinero=1u<<6,Ceguera=1u<<7,Espectadores=1u<<8,
    Impactos=1u<<9,Bomba=1u<<10,Awp=1u<<12,
    Armadura=1u<<13,Kit=1u<<14,Desactivando=1u<<15,Inmunidad=1u<<16,
    Casco=1u<<17,Recarga=1u<<18,ArmasSuelo=1u<<21,GranadasMundo=1u<<22,
    TiempoGranadas=1u<<24,FiltrarEquipo=1u<<25,PosicionRapida=1u<<26,
    ObservacionAutomatica=1u<<20
};

// Campos próximos en una sola llamada, sin dar por válidos valores parciales.
struct EstadoBasico {
    int salud=0;
    std::byte rellenoVida[campos::estadoVida-campos::salud-sizeof(int)]{};
    std::uint8_t vida=1;
    std::byte rellenoEquipo[campos::equipo-campos::estadoVida-1]{};
    std::uint8_t equipo=0;
};
static_assert(offsetof(EstadoBasico,vida)==campos::estadoVida-campos::salud);
static_assert(offsetof(EstadoBasico,equipo)==campos::equipo-campos::salud);

struct Municion { int cargador=-1; int reservado=0; int reserva=-1; };
static_assert(offsetof(Municion,reserva)==campos::balasReserva-campos::balasCargador);

class PaginasEntidades {
    std::uintptr_t lista=0;
    std::array<std::uintptr_t,64> paginas{};
    std::array<bool,64> leidas{};
public:
    void Reiniciar(std::uintptr_t nuevaLista) { lista=nuevaLista; leidas.fill(false); }
    template<class Lector>
    std::uintptr_t Obtener(std::uintptr_t origen,std::size_t indice,Lector&& leer) {
        if(!origen||indice>=paginas.size()) return 0;
        const auto direccion=origen+0x10+8*indice;
        if(lista!=origen) return leer(direccion);
        if(!leidas[indice]) { paginas[indice]=leer(direccion); leidas[indice]=true; }
        return paginas[indice];
    }
};

template<class Lector>
bool LeerHandlesInventario(std::uintptr_t personaje,std::array<std::uint32_t,64>& handles,int& cantidad,Lector&& leer) {
    cantidad=0; handles.fill(0);
    std::uintptr_t servicio=0;
    struct VectorInventario { int cantidad=0; int relleno=0; std::uintptr_t datos=0; } vector;
    static_assert(offsetof(VectorInventario,datos)==8);
    if(!personaje||!leer(personaje+campos::servicioArmas,&servicio,sizeof(servicio))||!servicio||
       !leer(servicio+campos::inventarioArmas,&vector,sizeof(vector))||vector.cantidad<0||vector.cantidad>int(handles.size())) return false;
    if(vector.cantidad&&(!vector.datos||!leer(vector.datos,handles.data(),vector.cantidad*sizeof(handles[0])))) return false;
    cantidad=vector.cantidad;return true;
}

template<class Lector>
bool LeerMunicion(std::uintptr_t arma,int& cargador,int& reserva,Lector&& leer) {
    cargador=reserva=-1;
    Municion datos;
    if(!arma||!leer(arma+campos::balasCargador,datos)||datos.cargador<0||datos.cargador>250||datos.reserva<0||datos.reserva>1000) return false;
    cargador=datos.cargador; reserva=datos.reserva;
    return true;
}
}

// Variables globales del proceso del juego
extern HANDLE procesoJuego;
extern uintptr_t baseCliente;
extern int anchoPantalla;
extern int altoPantalla;
extern HWND ventanaJuego;

// Funciones de manejo de procesos del juego
enum class ResultadoMemoria { Disponible,EsperandoProceso,SinAcceso,EsperandoModulos,VersionIncompatible,EsperandoVentana };
ResultadoMemoria IniciarMemoria();
int TickJuego();
void LiberarMemoria();

// Variables globales de datos del juego
void CopiarEspectadores(std::vector<std::string>& nombres,std::uint64_t& ultimaMuestra);
bool CopiarDatosJuego(JugadorLocal& local,std::array<DatosJugador,65>* jugadores=nullptr,std::uint64_t* muestra=nullptr);
void EstablecerPausa(bool enPausa);
bool JuegoEnPausa();
bool VisualesActivos();
void EstablecerVisualesActivos(bool activos);
enum class EstadoPartida { Fuera, Cargando, EnPartida, Jugando, Espectando, Pausado, Desconocido };
bool PartidaActiva();

// Seleccionar datos, frecuencia en Hz y distancia máxima de objetos en metros.
void EstablecerLecturas(std::uint32_t opciones,int frecuencia=200,float distanciaObjetos=60.f);
std::uint32_t LecturasActivas();
float DistanciaObjetosActiva();
bool LeerMunicionArma(uintptr_t arma,int& cargador,int& reserva);
bool LeerTexto(uintptr_t direccion,char* salida,std::size_t capacidad);
bool LeerOrigenMundo(uintptr_t entidad,Vector3& origen);
bool LeerBloque(uintptr_t direccion,void* salida,std::size_t tamano);

// Plantilla para leer memoria
template <typename T> T Leer(uintptr_t direccion) {
  static_assert(std::is_trivially_copyable_v<T>);
  T bloque{};
  if (!LeerBloque(direccion,&bloque,sizeof(T))) return T{};
  return bloque;
}

// Plantilla para leer memoria de forma segura
template <typename T> bool LeerSeguro(uintptr_t direccion, T &salida) {
  static_assert(std::is_trivially_copyable_v<T>);
  T valor{};
  if (!LeerBloque(direccion,&valor,sizeof(T))) return false;
  salida = valor;
  return true;
}

// Funciones de control del hilo
void IniciarHiloMemoria();
void DetenerHiloMemoria();
bool HiloMemoriaActivo();

// Leer poses completas y actuales para proyectarlas; no reutilizar bloques de huesos fallidos.
bool LeerPoseJugador(DatosJugador& jugador,PoseJugador& pose);
// Detectar la separación entre entidades mediante el controlador local conocido.
uintptr_t SeparacionEntidades();
