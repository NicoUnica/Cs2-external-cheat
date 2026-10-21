#pragma once
#include <Windows.h>
#include <cstdint>
#include "direcciones.hpp"
#include "tipos.hpp"
#include <mutex>
#include <string>
#include <vector>
#include <type_traits>
#include <array>
#include <cstring>

// Variables globales del proceso del juego
extern HANDLE procesoJuego;
extern uintptr_t baseCliente;
extern int anchoPantalla;
extern int altoPantalla;
extern HWND ventanaJuego;

// Funciones de manejo de procesos del juego
bool IniciarMemoria();
int TickJuego();
void LiberarMemoria();

// Variables globales de datos del juego
extern DatosJugador jugadoresGlobales[65];
extern JugadorLocal jugadorLocalGlobal;
extern std::vector<std::string> espectadoresGlobales;
extern std::mutex mutexDatos;
extern std::uint64_t datosLeidosEn,espectadoresLeidosEn;
bool CopiarDatosJuego(JugadorLocal& local,std::array<DatosJugador,65>* jugadores=nullptr);
void EstablecerPausa(bool enPausa);
bool JuegoEnPausa();
enum class EstadoPartida { Fuera, Cargando, EnPartida, Jugando, Espectando, Pausado, Desconocido };
struct InformacionPartida {
    EstadoPartida estado=EstadoPartida::Fuera;
    std::string mapa;
    std::uint64_t comprobadaEn=0;
};
InformacionPartida CopiarInformacionPartida();
bool PartidaActiva();
std::string TextoEstadoPartida();

namespace lecturas {
enum Opcion : std::uint32_t {
    Jugadores=1u<<0,Nombre=1u<<1,Arma=1u<<2,Apuntado=1u<<4,
    Portador=1u<<5,Dinero=1u<<6,Ceguera=1u<<7,Espectadores=1u<<8,
    Impactos=1u<<9,Bomba=1u<<10,Awp=1u<<12,
    Armadura=1u<<13,Kit=1u<<14,Desactivando=1u<<15,Inmunidad=1u<<16,
    Recarga=1u<<18,ArmasSuelo=1u<<21,GranadasMundo=1u<<22,
    TiempoGranadas=1u<<24,FiltrarEquipo=1u<<25,PosicionRapida=1u<<26,Sonido=1u<<27
};
}
// Opciones de adquisición de datos, frecuencia en Hz y distancia máxima en metros.
void EstablecerLecturas(std::uint32_t opciones,int frecuencia=200,float distanciaObjetos=60.f);
std::uint32_t LecturasActivas();
float DistanciaObjetosActiva();
bool LeerTexto(uintptr_t direccion,char* salida,std::size_t capacidad);
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
