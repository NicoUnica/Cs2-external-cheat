#include "juego.hpp"
#include <TlHelp32.h>
#include <Psapi.h>
#include "tipos.hpp"
#include "direcciones.hpp"
#include <bit>

#include "datos.hpp"
#include <Windows.h>
#include <atomic>
#include <algorithm>
#include <array>
#include <iterator>
#include <cstring>
#include <cstddef>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace {
// Separar la vista del personaje evita apagar el ESP al morir.
// La vista observada conserva su propio origen y equipo.
template<class Lector,class Resolver>
void ActualizarVistaJugador(JugadorLocal& local,uintptr_t controlador,Lector&& leer,Resolver&& resolver) {
    local.personajeVista=0; local.origenVista={}; local.equipoVista=0; local.vistaValida=false;
    auto LeerHandle=[&](uintptr_t direccion) { uint32_t handle=0; leer(direccion,handle); return handle; };
    auto LeerPuntero=[&](uintptr_t direccion) { uintptr_t valor=0; leer(direccion,valor); return valor; };
    if(local.esValido) {
        local.personajeVista=local.personaje; local.origenVista=local.origen;
        local.equipoVista=local.equipo; local.vistaValida=true;
    } else if(controlador) {
        // m_hObserverPawn es independiente del cadáver y cambia al entrar en observación.
        const auto observador=resolver(LeerHandle(controlador+campos::personajeObservador));
        const auto fuente=observador ? observador : local.personaje;
        const auto servicio=fuente ? LeerPuntero(fuente+campos::servicioObservacion) : 0;
        // Modos de observación del volcado: primera persona=2, seguimiento=3, cámara libre=4.
        uint8_t modo=0;
        if(servicio&&leer(servicio+campos::modoObservacion,modo)) {
            local.modoObservacion=modo;
            uintptr_t vista=0;
            if(modo==2||modo==3) {
                vista=resolver(LeerHandle(servicio+campos::objetivoObservacion));
                int salud=0; uint8_t vida=1;
                if(!vista||!leer(vista+campos::salud,salud)||salud<=0||salud>100||
                    !leer(vista+campos::estadoVida,vida)||vida!=0) vista=0;
            } else if(modo==4) vista=fuente; // Cámara libre: no se reutiliza la posición del cadáver.
            Vector3 origen{}; uint8_t equipo=0;
            if(vista&&leer(vista+campos::origenAnterior,origen)&&
                std::isfinite(origen.x)&&std::isfinite(origen.y)&&std::isfinite(origen.z)) {
                if(modo==4) leer(controlador+campos::equipo,equipo);
                else leer(vista+campos::equipo,equipo);
                local.personajeVista=vista; local.origenVista=origen;
                local.equipoVista=equipo; local.vistaValida=true;
            }
        }
    }
}

namespace validacion {
// Reintentar tras fallos repetidos, sin invalidar las demás entidades.
struct Reintento {
    std::uintptr_t identidad=0;
    std::uint64_t intentarEn=0;
    unsigned fallos=0;
    bool Disponible(std::uintptr_t actual,std::uint64_t ahora) {
        if(actual!=identidad) { identidad=actual; intentarEn=0; fallos=0; }
        return ahora>=intentarEn;
    }
    void Resultado(bool valido,std::uint64_t ahora) {
        if(valido) { fallos=0; intentarEn=0; }
        else if(++fallos>=8) { intentarEn=ahora+1000; fallos=0; }
    }
};
}

}

HANDLE procesoJuego = nullptr;
uintptr_t baseCliente = 0;
static uintptr_t baseMotor=0;
int TickJuego() {
    if(!baseMotor) return 0;
    const auto red=Leer<uintptr_t>(baseMotor+direcciones::globales::motor::clienteRed);
    int tick=0;
    return baseMotor&&red&&LeerSeguro(red+direcciones::globales::motor::tickCliente,tick)&&tick>0 ? tick : 0;
}
int anchoPantalla = 1920;
int altoPantalla = 1080;
HWND ventanaJuego = nullptr;

namespace {
class RecursoAutomatico {
  HANDLE valor;
public:
  explicit RecursoAutomatico(HANDLE control) : valor(control) {}
  ~RecursoAutomatico() { if (valor && valor!=INVALID_HANDLE_VALUE) CloseHandle(valor); }
  RecursoAutomatico(const RecursoAutomatico&)=delete;
  RecursoAutomatico& operator=(const RecursoAutomatico&)=delete;
  HANDLE Obtener() const { return valor; }
  explicit operator bool() const { return valor && valor!=INVALID_HANDLE_VALUE; }
};
}

// Obtener el identificador del proceso por su nombre
static DWORD BuscarProceso(const wchar_t *nombreProceso) {
  DWORD identificadorProceso = 0;
  if (!nombreProceso || !*nombreProceso) return 0;
  RecursoAutomatico listaTemporal(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!listaTemporal)
    return 0;
  PROCESSENTRY32W entradaProceso{};
  entradaProceso.dwSize = sizeof(entradaProceso);
  if (Process32FirstW(listaTemporal.Obtener(), &entradaProceso)) {
    do {
      if (_wcsicmp(entradaProceso.szExeFile, nombreProceso) == 0) {
        identificadorProceso = entradaProceso.th32ProcessID;
        break;
      }
    } while (Process32NextW(listaTemporal.Obtener(), &entradaProceso));
  }
  return identificadorProceso;
}

// Abrir proceso del juego con los permisos de lectura necesarios.
static HANDLE AbrirProcesoJuego(DWORD identificadorProceso) {
  return OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                     FALSE, identificadorProceso);
}

// Obtener dirección base de un módulo del proceso
static uintptr_t BuscarModulo(HANDLE proceso, const wchar_t *nombreModulo) {
  if (!proceso || proceso==INVALID_HANDLE_VALUE || !nombreModulo || !*nombreModulo) return 0;
  const DWORD identificadorProceso=GetProcessId(proceso);
  if (!identificadorProceso) return 0;
  RecursoAutomatico listaTemporal(CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, identificadorProceso));
  MODULEENTRY32W entradaModulo{};
  entradaModulo.dwSize = sizeof(entradaModulo);
  uintptr_t baseModulo = 0;
  if (listaTemporal&&Module32FirstW(listaTemporal.Obtener(), &entradaModulo)) {
    do {
      if (_wcsicmp(entradaModulo.szModule, nombreModulo) == 0) {
        baseModulo = (uintptr_t)entradaModulo.modBaseAddr;
        break;
      }
    } while (Module32NextW(listaTemporal.Obtener(), &entradaModulo));
  }
  // Algunos procesos permiten consultar y leer sus módulos, pero rechazan la instantánea Toolhelp.
  if(!baseModulo) {
    std::array<HMODULE,1024> modulos{};DWORD bytes=0;
    if(K32EnumProcessModulesEx(proceso,modulos.data(),static_cast<DWORD>(sizeof(modulos)),&bytes,LIST_MODULES_ALL)&&bytes<=sizeof(modulos)) {
      for(std::size_t i=0;i<bytes/sizeof(HMODULE);++i) {
        wchar_t nombre[256]{};
        if(K32GetModuleBaseNameW(proceso,modulos[i],nombre,static_cast<DWORD>(std::size(nombre)))&&_wcsicmp(nombre,nombreModulo)==0) {
          baseModulo=reinterpret_cast<uintptr_t>(modulos[i]);break;
        }
      }
    }
  }
  return baseModulo;
}

// Inicializar la lectura del juego y obtener los recursos del proceso.
ResultadoMemoria IniciarMemoria() {
  LiberarMemoria();
  DWORD identificadorProceso = BuscarProceso(L"cs2.exe");
  if (!identificadorProceso) {

    return ResultadoMemoria::EsperandoProceso;
  }

  procesoJuego = AbrirProcesoJuego(identificadorProceso);
  if (!procesoJuego || procesoJuego == INVALID_HANDLE_VALUE) {
    return ResultadoMemoria::SinAcceso;
  }

  baseCliente = BuscarModulo(procesoJuego, L"client.dll");
  if (!baseCliente) {

    LiberarMemoria();
    return ResultadoMemoria::EsperandoModulos;
  }
  baseMotor=BuscarModulo(procesoJuego,L"engine2.dll");
  int compilacion=0;
  if(!baseMotor||!LeerSeguro(baseMotor+direcciones::globales::motor::compilacion,compilacion)) {
    LiberarMemoria(); return ResultadoMemoria::EsperandoModulos;
  }
  if(compilacion!=direcciones::globales::motor::compilacionEsperada) {

    LiberarMemoria(); return ResultadoMemoria::VersionIncompatible;
  }

  {
    ventanaJuego = FindWindowW(L"SDL_app", L"Counter-Strike 2");
    if (!ventanaJuego) ventanaJuego = FindWindowW(nullptr, L"Counter-Strike 2");
    
    DWORD propietario=0;
    if(ventanaJuego) GetWindowThreadProcessId(ventanaJuego,&propietario);
    if(propietario!=identificadorProceso) ventanaJuego=nullptr;
    
    struct DatosVentana { DWORD identificadorProceso; HWND ventana; };
    DatosVentana data = { identificadorProceso, nullptr };
    if(!ventanaJuego) EnumWindows([](HWND ventana, LPARAM local) -> BOOL {
      DatosVentana* datos = (DatosVentana*)local;
      DWORD identificadorVentana;
      GetWindowThreadProcessId(ventana, &identificadorVentana);
      if (identificadorVentana == datos->identificadorProceso && IsWindowVisible(ventana)) {
        datos->ventana = ventana;
        return FALSE; 
      }
      return TRUE;
    }, (LPARAM)&data);
    
    if (data.ventana) {
        ventanaJuego = data.ventana;
    }
  }

  if (!ventanaJuego) {

    LiberarMemoria();
    return ResultadoMemoria::EsperandoVentana;
  }

  RECT cliente{};
  if(GetClientRect(ventanaJuego,&cliente)&&cliente.right>0&&cliente.bottom>0) {
    anchoPantalla=cliente.right; altoPantalla=cliente.bottom;
  }
  return ResultadoMemoria::Disponible;
}



// Limpiar recursos de memoria
void LiberarMemoria() {
  if (procesoJuego && procesoJuego != INVALID_HANDLE_VALUE)
    CloseHandle(procesoJuego);
  procesoJuego = nullptr;
  baseCliente = 0;
  baseMotor = 0;
  ventanaJuego = nullptr;
}

static DatosJugador jugadoresGlobales[65] = {};
static JugadorLocal jugadorLocalGlobal = {};
static std::vector<std::string> espectadoresGlobales;
static std::mutex mutexDatos;
static std::uint64_t datosLeidosEn=0,espectadoresLeidosEn=0;
static std::atomic<bool> juegoPausado{false};
static std::atomic<bool> visualesActivos{true};
bool VisualesActivos() { return visualesActivos.load(std::memory_order_relaxed); }
// Estado y antigüedad se publican en una única operación coherente.
static std::atomic<std::uint64_t> estadoPartida{0};
static_assert(static_cast<unsigned>(EstadoPartida::Desconocido)<8);
static EstadoPartida EstadoPublicado() { return static_cast<EstadoPartida>(estadoPartida.load(std::memory_order_relaxed)&7u); }
static void PublicarEstadoPartida(EstadoPartida estado,std::uint64_t comprobadaEn=0) {
    estadoPartida.store((comprobadaEn<<3)|static_cast<std::uint64_t>(estado),std::memory_order_relaxed);
}
static EstadoPartida ObtenerEstadoPartida() {
    if(JuegoEnPausa()||!VisualesActivos()) return EstadoPartida::Pausado;
    const auto publicado=estadoPartida.load(std::memory_order_relaxed);
    const auto estado=static_cast<EstadoPartida>(publicado&7u);
    const auto comprobadoEn=publicado>>3;
    if(estado!=EstadoPartida::Fuera&&(!comprobadoEn||GetTickCount64()-comprobadoEn>1500)) return EstadoPartida::Desconocido;
    return estado;
}
bool PartidaActiva() {
    const auto estado=ObtenerEstadoPartida();
    return estado==EstadoPartida::Jugando||estado==EstadoPartida::Espectando;
}

static std::mutex mutexSuspension;
static std::condition_variable despertarLector;
void EstablecerVisualesActivos(bool activos) {
    {
        std::lock_guard<std::mutex> bloqueo(mutexSuspension);
        visualesActivos.store(activos,std::memory_order_relaxed);
    }
    despertarLector.notify_all();
}
void EstablecerPausa(bool enPausa) {
    if(juegoPausado.load(std::memory_order_relaxed)==enPausa) return;
    {
        std::lock_guard<std::mutex> bloqueo(mutexSuspension);
        if(juegoPausado.exchange(enPausa,std::memory_order_relaxed)==enPausa) return;
    }
    despertarLector.notify_all();
}
bool JuegoEnPausa() { return juegoPausado.load(std::memory_order_relaxed); }
void CopiarEspectadores(std::vector<std::string>& nombres,std::uint64_t& ultimaMuestra) {
    std::lock_guard<std::mutex> bloqueo(mutexDatos);
    if(MuestraReciente(espectadoresLeidosEn,GetTickCount64(),600)) {
        if(ultimaMuestra!=espectadoresLeidosEn) { nombres=espectadoresGlobales; ultimaMuestra=espectadoresLeidosEn; }
    } else { nombres.clear(); ultimaMuestra=0; }
}
bool CopiarDatosJuego(JugadorLocal& local,std::array<DatosJugador,65>* jugadores,std::uint64_t* muestra) {
    std::lock_guard<std::mutex> bloqueo(mutexDatos);
    if(muestra) *muestra=datosLeidosEn;
    if (!MuestraReciente(datosLeidosEn,GetTickCount64())) {
        local={}; if (jugadores) *jugadores={}; return false;
    }
    local=jugadorLocalGlobal;
    if (jugadores) std::copy(std::begin(jugadoresGlobales),std::end(jugadoresGlobales),jugadores->begin());
    return true;
}

static std::atomic<bool> memoriaActiva(false);
static std::thread hiloMemoria;
static std::mutex mutexHilo;
static std::atomic<std::uint64_t> opcionesLectura{0};
static std::atomic<int> intervaloLecturas{5};
std::uint32_t LecturasActivas() { return static_cast<std::uint32_t>(opcionesLectura.load(std::memory_order_relaxed))&0x0FFFFFFFu; }
float DistanciaObjetosActiva() { return std::bit_cast<float>(static_cast<std::uint32_t>(opcionesLectura.load(std::memory_order_relaxed)>>32)); }
static int IntervaloLecturas() { return intervaloLecturas.load(std::memory_order_relaxed); }
void EstablecerLecturas(std::uint32_t opciones,int frecuencia,float distanciaObjetos) {
    const auto hz=std::clamp(frecuencia,10,250);
    intervaloLecturas.store((1000+hz-1)/hz,std::memory_order_relaxed);
    const float metros=std::isfinite(distanciaObjetos) ? std::clamp(distanciaObjetos,1.f,10000.f) : 60.f;
    const auto distancia=static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(metros));
    opcionesLectura.store((distancia<<32)|(opciones&0x0FFFFFFFu),std::memory_order_relaxed);
}
bool LeerOrigenMundo(uintptr_t entidad,Vector3& origen) {
    const auto escena=Leer<uintptr_t>(entidad+campos::nodoEscena);
    return escena&&LeerSeguro(escena+campos::origenAbsoluto,origen)&&PosicionMundoValida(origen);
}
bool LeerBloque(uintptr_t direccion,void* salida,std::size_t tamano) {
    if (!VisualesActivos()||!salida || !tamano || !direccion || !procesoJuego || procesoJuego==INVALID_HANDLE_VALUE ||
        tamano-1>UINTPTR_MAX-direccion) return false;
    SIZE_T bytes=0;
    const bool correcto=ReadProcessMemory(procesoJuego,reinterpret_cast<LPCVOID>(direccion),salida,tamano,&bytes) && bytes==tamano;
    return correcto;
}
bool LeerTexto(uintptr_t direccion,char* salida,std::size_t capacidad) {
    if (!salida||!capacidad) return false;
    salida[0]=0;
    if (capacidad<2) return false;
    static const std::size_t tamanoPagina=[]() { SYSTEM_INFO informacionSistema{}; GetSystemInfo(&informacionSistema); return informacionSistema.dwPageSize; }();
    for (std::size_t desplazamiento=0;desplazamiento<capacidad-1;) {
        if (desplazamiento>UINTPTR_MAX-direccion) { salida[0]=0; return false; }
        const auto actual=direccion+desplazamiento;
        const auto cantidad=(std::min)({std::size_t{32},capacidad-1-desplazamiento,tamanoPagina-actual%tamanoPagina});
        if (!LeerBloque(actual,salida+desplazamiento,cantidad)) { salida[0]=0; return false; }
        if (std::memchr(salida+desplazamiento,0,cantidad)) return true;
        desplazamiento+=cantidad;
    }
    salida[capacidad-1]=0;
    return true;
}
static std::atomic<uintptr_t> separacionEntidades{0x70};
uintptr_t SeparacionEntidades() { return separacionEntidades.load(); }
namespace {
thread_local lecturas::PaginasEntidades paginasActuales;
uintptr_t PaginaEntidades(uintptr_t lista,std::size_t indice) {
    return paginasActuales.Obtener(lista,indice,[](uintptr_t direccion) { return Leer<uintptr_t>(direccion); });
}
}
bool LeerMunicionArma(uintptr_t arma,int& cargador,int& reserva) {
    return lecturas::LeerMunicion(arma,cargador,reserva,[](uintptr_t direccion,auto& datos) { return LeerSeguro(direccion,datos); });
}
static void ValidarListaEntidades(uintptr_t lista,uintptr_t controlador) {
    static ULONGLONG comprobadoEn=0;
    static uintptr_t listaAnterior=0,controladorAnterior=0;
    static int indiceAnterior=0;
    const auto ahora=GetTickCount64();
    if(lista!=listaAnterior||controlador!=controladorAnterior) { comprobadoEn=0; indiceAnterior=0; listaAnterior=lista; controladorAnterior=controlador; }
    if (!controlador||ahora-comprobadoEn<1000) return;
    comprobadoEn=ahora;
    const auto bloqueEntidades=PaginaEntidades(lista,0);
    if (!bloqueEntidades) return;
    if(indiceAnterior&&Leer<uintptr_t>(bloqueEntidades+SeparacionEntidades()*indiceAnterior)==controlador) return;
    std::array<std::byte,64*0x78+sizeof(uintptr_t)> bloque{};
    if(!LeerBloque(bloqueEntidades,bloque.data(),bloque.size())) return;
    for (const uintptr_t separacion : {uintptr_t{0x70},uintptr_t{0x78}})
        for (int indice=1;indice<=64;++indice) {
            uintptr_t candidato=0; std::memcpy(&candidato,bloque.data()+separacion*indice,sizeof(candidato));
            if(candidato==controlador) { separacionEntidades=separacion; indiceAnterior=indice; return; }
        }
}

static uintptr_t ResolverEntidad(std::uint32_t control,uintptr_t lista) {
    if (!lista||!control||control==0xFFFFFFFF) return 0;
    const auto indice=control&0x7FFF;
    const auto bloqueEntidades=PaginaEntidades(lista,indice>>9);
    return bloqueEntidades ? Leer<uintptr_t>(bloqueEntidades+SeparacionEntidades()*(indice&0x1FF)) : 0;
}
static std::uint16_t IdentificadorArma(uintptr_t arma) {
    return arma ? Leer<std::uint16_t>(arma+campos::atributos+campos::objeto+campos::definicionObjeto) : 0;
}
static uintptr_t ArmaActiva(uintptr_t personaje,uintptr_t lista) {
    const auto servicio=Leer<uintptr_t>(personaje+campos::servicioArmas);
    return servicio ? ResolverEntidad(Leer<std::uint32_t>(servicio+campos::armaActiva),lista) : 0;
}
struct EntidadGuardada {
    uint32_t identificadorPersonaje=0;
    uintptr_t direccionPersonaje=0,controlador=0,arma=0;
    std::uint16_t idArma=0;
    std::uint64_t idArmaLeidoEn=0;
    std::uint64_t nombreLeidoEn=0,informacionLeidaEn=0,inventarioLeidoEn=0,armaLeidaEn=0,datosLentosLeidosEn=0;
    std::uint64_t municionLeidaEn=0;
    int balasCargador=-1,balasReserva=-1;
};
static void LeerNombreArma(uintptr_t entidadArma, char *salida, size_t longitudMaxima) {
    salida[0]=0;
    if (!entidadArma) return;
    uintptr_t identidad = Leer<uintptr_t>(entidadArma + campos::identidad);
    if (!identidad) return;

    uintptr_t direccionNombre = Leer<uintptr_t>(identidad + campos::nombreInterno);
    if (!direccionNombre) return;

    char textoOriginal[128] = {};
    if (!LeerTexto(direccionNombre,textoOriginal,sizeof(textoOriginal))) return;

    const char *origenTexto = textoOriginal;
    if (strncmp(origenTexto, "weapon_", 7) == 0) origenTexto += 7;

    strncpy_s(salida, longitudMaxima, origenTexto, longitudMaxima-1);
    if (salida[0] >= 'a' && salida[0] <= 'z') salida[0] -= 32;
}

static bool LlevaBomba(uintptr_t personaje,uintptr_t lista) {
    int cantidad=0;
    std::array<std::uint32_t,64> identificadores{};
    if(!lecturas::LeerHandlesInventario(personaje,identificadores,cantidad,LeerBloque)) return false;
    for (int i=0;i<cantidad;++i) if (IdentificadorArma(ResolverEntidad(identificadores[i],lista))==49) return true;
    return false;
}
static bool LeerEstado(uintptr_t direccion) {
    std::uint8_t valor=0;
    return LeerSeguro(direccion,valor)&&valor==1;
}
static void LeerEstadoArma(DatosJugador& jugador,uintptr_t lista,std::uint32_t opciones,EntidadGuardada& guardado,std::uint64_t ahora) {
    jugador.apuntadoLeido=false; jugador.estaApuntando=false;
    std::uint8_t apuntando=0;
    jugador.apuntadoLeido=(opciones&lecturas::Apuntado)&&LeerSeguro(jugador.personaje+campos::apuntando,apuntando)&&apuntando<=1;
    jugador.estaApuntando=jugador.apuntadoLeido&&apuntando!=0;
    jugador.armaId=0; jugador.recargando=false;
    jugador.balasCargador=jugador.balasReserva=-1;
    if (!(opciones&(lecturas::Arma|lecturas::Recarga))) return;
    const auto arma=ArmaActiva(jugador.personaje,lista);
    const bool armaCambiada=arma!=guardado.arma;
    if(arma!=guardado.arma) {
        guardado.idArma=0;
        guardado.idArmaLeidoEn=0;
        guardado.municionLeidaEn=0;
        guardado.balasCargador=guardado.balasReserva=-1;
    }
    guardado.arma=arma;
    if(!guardado.idArmaLeidoEn||ahora-guardado.idArmaLeidoEn>=250) {
        guardado.idArma=IdentificadorArma(arma); guardado.idArmaLeidoEn=ahora;
    }
    if ((opciones&lecturas::Arma)&&(armaCambiada||ahora-guardado.armaLeidaEn>=250||!jugador.nombreArma[0])) {
        const auto* dato=armas::Buscar(guardado.idArma);
        if(dato) strncpy_s(jugador.nombreArma,dato->nombre,_TRUNCATE);
        else LeerNombreArma(arma,jugador.nombreArma,sizeof(jugador.nombreArma));
        guardado.armaLeidaEn=ahora;
    }
    jugador.armaId=guardado.idArma;
    const bool armaConCargador=arma&&armas::PuedeRecargar(jugador.armaId);
    if (armaConCargador&&(opciones&lecturas::Recarga)) jugador.recargando=LeerEstado(arma+campos::recargando);
    if(armaConCargador&&(opciones&lecturas::Arma)) {
        if(!guardado.municionLeidaEn||ahora-guardado.municionLeidaEn>=100) {
            int cargador=-1,reserva=-1;
            const bool valido=LeerMunicionArma(arma,cargador,reserva);
            guardado.balasCargador=valido ? cargador : -1;
            guardado.balasReserva=valido ? reserva : -1;
            guardado.municionLeidaEn=ahora;
        }
        jugador.balasCargador=guardado.balasCargador;
        jugador.balasReserva=guardado.balasReserva;
    }
}

static EntidadGuardada entidadesGuardadas[65] = {};
struct BaseEspectador { uintptr_t controlador=0,personaje=0; std::uint32_t identificador=0; int salud=0; std::uint8_t equipo=0; bool valida=false; };
static std::array<BaseEspectador,65> baseEspectadores{};
static std::uint64_t baseEspectadoresEn=0;
static uintptr_t baseEspectadoresLista=0;

struct ArmaLocalGuardada {
    uintptr_t personaje=0,lista=0,arma=0;
    std::uint16_t id=0;
    std::uint64_t comprobadaEn=0,idLeidoEn=0;
};
static ArmaLocalGuardada armaLocalGuardada;
static void VaciarDatosJugadores() {
    std::lock_guard<std::mutex> bloqueo(mutexDatos);
    jugadorLocalGlobal = {}; armaLocalGuardada={};
    espectadoresGlobales.clear();
    datosLeidosEn=espectadoresLeidosEn=0;
    baseEspectadores={}; baseEspectadoresEn=0; baseEspectadoresLista=0;
    for (auto& jugador : jugadoresGlobales) jugador = {};
    for (auto& entidad : entidadesGuardadas) entidad = {};
    ReiniciarDatosExtras();
    mundo::Reiniciar();
}

// Sondeo ligero independiente de las opciones visuales; solo el lector escribe esta caché.
struct SondeoPartida {
    std::uint64_t comprobadoEn=0;
    uintptr_t red=0;
    bool conectada=false;
    EstadoPartida estado=EstadoPartida::Fuera;
};
static SondeoPartida sondeoPartida;
static bool ComprobarPartida() {
    const auto ahora=GetTickCount64();
    const auto intervalo=sondeoPartida.conectada ? 100ULL : 250ULL;
    if(sondeoPartida.comprobadoEn&&ahora-sondeoPartida.comprobadoEn<intervalo) return sondeoPartida.conectada;
    sondeoPartida.comprobadoEn=ahora;
    uintptr_t red=0; int conexion=0; std::uint8_t fondo=1;
    const bool valida=baseMotor&&LeerSeguro(baseMotor+direcciones::globales::motor::clienteRed,red)&&red&&
        LeerSeguro(red+direcciones::globales::motor::estadoConexion,conexion)&&conexion>=0&&conexion<=7&&
        LeerSeguro(red+direcciones::globales::motor::mapaFondo,fondo)&&fondo<=1;
    const bool conectadaAntes=sondeoPartida.conectada;
    const bool cambioConexion=red!=sondeoPartida.red;
    sondeoPartida.red=red;
    sondeoPartida.conectada=valida&&conexion==6&&!fondo;
    sondeoPartida.estado=!valida ? EstadoPartida::Desconocido : fondo||conexion==0 ? EstadoPartida::Fuera :
        sondeoPartida.conectada ? EstadoPartida::EnPartida : EstadoPartida::Cargando;
    if(sondeoPartida.conectada&&cambioConexion&&conectadaAntes) {
        PublicarEstadoPartida(EstadoPartida::EnPartida,ahora); VaciarDatosJugadores();
    }
    return sondeoPartida.conectada;
}

static void InvalidarPartida() {
    // Impedir nuevas lecturas desde el dibujo antes de retirar las muestras publicadas.
    const auto anterior=EstadoPublicado();
    PublicarEstadoPartida(sondeoPartida.estado,sondeoPartida.comprobadoEn);
    if(anterior==EstadoPartida::Jugando||anterior==EstadoPartida::Espectando) VaciarDatosJugadores();
}

// Actualizar caché de entidades del juego
static void ActualizarJugadores() {
    if(!ComprobarPartida()) { InvalidarPartida(); return; }
    const auto opciones=LecturasActivas();
    // Las muestras se invalidan al reactivar y los consumidores recuperan sus cachés.
    if (!procesoJuego || !baseCliente) { InvalidarPartida(); return; }

    uintptr_t listaEntidades = Leer<uintptr_t>(baseCliente + direcciones::globales::cliente::listaEntidades);
    if (!listaEntidades) { InvalidarPartida(); return; }

    uintptr_t controladorLocal = Leer<uintptr_t>(baseCliente + direcciones::globales::cliente::controladorLocal);
    uintptr_t personajeLocal = Leer<uintptr_t>(baseCliente + direcciones::globales::cliente::personajeLocal);
    paginasActuales.Reiniciar(listaEntidades);
    ValidarListaEntidades(listaEntidades,controladorLocal);
    const auto ahora=GetTickCount64();
    static std::uint32_t opcionesAnteriores=0;
    const bool opcionesCambiadas=opciones!=opcionesAnteriores;
    opcionesAnteriores=opciones;

    using EstadoSalud=lecturas::EstadoBasico;
    JugadorLocal local = {};
    local.controlador=controladorLocal;
    if (personajeLocal && controladorLocal) {
        local.personaje = personajeLocal;
        EstadoSalud estadoLocal{};
        const bool saludLeida=LeerSeguro(personajeLocal+campos::salud,estadoLocal);
        local.salud=saludLeida ? estadoLocal.salud : 0;
        local.equipo=saludLeida ? estadoLocal.equipo : 0;
        uint8_t personajeVivo=0;
        local.esValido = (saludLeida && local.salud > 0 && local.salud <= 100 &&
            LeerSeguro(controladorLocal+campos::personajeVivo,personajeVivo)&&personajeVivo==1&&
            estadoLocal.vida==0&&
            local.equipo<=3 &&
            LeerSeguro(personajeLocal + campos::origenAnterior, local.origen) &&
            std::isfinite(local.origen.x) && std::isfinite(local.origen.y) && std::isfinite(local.origen.z));
        if(opciones&lecturas::Awp) {
            auto& guardado=armaLocalGuardada;
            if(guardado.personaje!=personajeLocal||guardado.lista!=listaEntidades||opcionesCambiadas) {
                guardado={}; guardado.personaje=personajeLocal; guardado.lista=listaEntidades;
            }
            if(!guardado.comprobadaEn||ahora-guardado.comprobadaEn>=30) {
                const auto arma=ArmaActiva(personajeLocal,listaEntidades);
                guardado.comprobadaEn=ahora;
                if(arma!=guardado.arma||!guardado.idLeidoEn||ahora-guardado.idLeidoEn>=250) {
                    guardado.arma=arma; guardado.id=IdentificadorArma(arma); guardado.idLeidoEn=ahora;
                }
            }
            local.identificadorArma=guardado.id;
            if(local.esValido&&(guardado.id==9||guardado.id==40)) {
                std::uint8_t apuntando=0;
                local.apuntadoLeido=LeerSeguro(personajeLocal+campos::apuntando,apuntando)&&apuntando<=1;
                local.estaApuntando=local.apuntadoLeido&&apuntando!=0;
            }
        } else armaLocalGuardada={};
    }

    ActualizarVistaJugador(local,controladorLocal,
        [](uintptr_t direccion,auto& valor) { return LeerSeguro(direccion,valor); },
        [listaEntidades](uint32_t handle) { return ResolverEntidad(handle,listaEntidades); });
    if(!(opciones&lecturas::ObservacionAutomatica)&&!local.esValido) local.equipoVista=local.equipo;

    if(!local.vistaValida) { InvalidarPartida(); return; }
    PublicarEstadoPartida(local.esValido ? EstadoPartida::Jugando : EstadoPartida::Espectando,sondeoPartida.comprobadoEn);

    if (!(opciones&lecturas::Jugadores)) {
        std::lock_guard<std::mutex> bloqueo(mutexDatos);
        jugadorLocalGlobal=local;
        datosLeidosEn=GetTickCount64();
        if (opcionesCambiadas) {
            for (auto& jugador:jugadoresGlobales) jugador={};
            for (auto& guardado:entidadesGuardadas) guardado={};
        }
        return;
    }

    DatosJugador jugadoresAnteriores[65]{};
    {
        std::lock_guard<std::mutex> bloqueo(mutexDatos);
        memcpy(jugadoresAnteriores, jugadoresGlobales, sizeof(jugadoresAnteriores));
    }

    DatosJugador jugadoresNuevos[65] = {};
    baseEspectadores={};
    const uintptr_t entrada=PaginaEntidades(listaEntidades,0);
    const auto separacion=SeparacionEntidades();
    std::array<std::byte,64*256> controladores;
    const bool bloqueValido=entrada&&separacion>=sizeof(uintptr_t)&&separacion<=256&&
        LeerBloque(entrada+separacion,controladores.data(),63*separacion+sizeof(uintptr_t));
    for (int i = 1; i <= 64; i++) {
        if (!entrada) { entidadesGuardadas[i] = {}; continue; }

        uintptr_t controlador=0;
        if(bloqueValido) std::memcpy(&controlador,controladores.data()+separacion*(i-1),sizeof(controlador));
        else controlador=Leer<uintptr_t>(entrada+separacion*i);
        if (!controlador || controlador == controladorLocal) { entidadesGuardadas[i] = {}; continue; }

        uint32_t identificadorPersonaje = Leer<uint32_t>(controlador + campos::personaje);
        if (!identificadorPersonaje || identificadorPersonaje == 0xFFFFFFFF) {
            entidadesGuardadas[i] = {0,0,0};
            continue;
        }

        uintptr_t personaje = 0;
        if (entidadesGuardadas[i].controlador == controlador &&
            entidadesGuardadas[i].identificadorPersonaje == identificadorPersonaje && entidadesGuardadas[i].direccionPersonaje != 0) {
            personaje = entidadesGuardadas[i].direccionPersonaje;
        } else {
            uint32_t indicePersonaje = identificadorPersonaje & 0x7FFF;
            uintptr_t entradaPersonaje = (indicePersonaje>>9)==0 ? entrada : PaginaEntidades(listaEntidades,indicePersonaje>>9);
            if (entradaPersonaje) {
                personaje = Leer<uintptr_t>(entradaPersonaje + SeparacionEntidades() * (indicePersonaje & 0x1FF));
                entidadesGuardadas[i] = {identificadorPersonaje, personaje, controlador};
            }
        }
        if (!personaje) { entidadesGuardadas[i] = {}; continue; }

        DatosJugador &p = jugadoresNuevos[i];
        p.personaje = personaje;
        p.controlador = controlador;
        EstadoSalud b1{};
        const bool estadoLeido=LeerSeguro(personaje + campos::salud, b1);
        if (estadoLeido) {
            p.salud = b1.salud;
            p.estaVivo = (b1.vida == 0 && b1.salud > 0);
            p.esValido = (b1.salud > 0 && b1.salud <= 100);
        }
        p.equipo=b1.equipo;
        if (!estadoLeido||p.equipo>3) {
            p.esValido = false;
            continue;
        }
        baseEspectadores[i]={controlador,personaje,identificadorPersonaje,p.salud,p.equipo,estadoLeido&&p.salud>=0&&p.salud<=100&&p.equipo<=3};
        if(local.OcultarJugador(p.personaje,p.equipo,(opciones&lecturas::FiltrarEquipo)!=0)) { p.esValido=false; continue; }
        if (!p.esValido) continue;

        const bool mismoJugador = jugadoresAnteriores[i].controlador == controlador && jugadoresAnteriores[i].personaje == personaje;

        if (!LeerSeguro(personaje + campos::origenAnterior, p.origen) ||
            !std::isfinite(p.origen.x) || !std::isfinite(p.origen.y) || !std::isfinite(p.origen.z)) {
            p.esValido = false;
            continue;
        }
        if (local.vistaValida) p.distancia = Distancia3D(local.origenVista, p.origen);
        p.posicionLeidaEn=ahora;

        auto& guardado=entidadesGuardadas[i];
        if (mismoJugador) {
            if (opciones&lecturas::Nombre) strncpy_s(p.nombre,jugadoresAnteriores[i].nombre,sizeof(p.nombre)-1);
            if (opciones&lecturas::Arma) strncpy_s(p.nombreArma,jugadoresAnteriores[i].nombreArma,sizeof(p.nombreArma)-1);
            if (opciones&lecturas::Apuntado) { p.estaApuntando=jugadoresAnteriores[i].estaApuntando; p.apuntadoLeido=jugadoresAnteriores[i].apuntadoLeido; }
            if (opciones&lecturas::Portador) p.llevaBomba=jugadoresAnteriores[i].llevaBomba;
            if (opciones&lecturas::Dinero) p.dinero=jugadoresAnteriores[i].dinero;
            const auto& previo=jugadoresAnteriores[i];
            p.cegado=previo.cegado; p.armadura=previo.armadura;
            p.casco=previo.casco; p.kit=previo.kit; p.desactivando=previo.desactivando; p.inmune=previo.inmune;
            p.recargando=previo.recargando;
            p.armaId=previo.armaId;
            p.balasCargador=previo.balasCargador;
            p.balasReserva=previo.balasReserva;
        }
        if ((opciones&lecturas::Nombre)&&(!mismoJugador||opcionesCambiadas||ahora-guardado.nombreLeidoEn>=1000)) {
            uintptr_t direccionNombre = Leer<uintptr_t>(controlador + campos::nombreJugador);
            LeerTexto(direccionNombre,p.nombre,sizeof(p.nombre)); guardado.nombreLeidoEn=ahora;
        }
        if (!mismoJugador||opcionesCambiadas||ahora-guardado.informacionLeidaEn>=20) {
            LeerEstadoArma(p,listaEntidades,opciones,guardado,ahora);
            p.cegado=false;
            if (opciones&lecturas::Ceguera) {
                float tiempo=0;
                p.cegado=LeerSeguro(personaje+campos::tiempoCeguera,tiempo)&&std::isfinite(tiempo)&&tiempo>0.f;
            }
            p.desactivando=(opciones&lecturas::Desactivando)&&LeerEstado(personaje+campos::desactivando);
            p.inmune=(opciones&lecturas::Inmunidad)&&LeerEstado(personaje+campos::inmune);
            guardado.informacionLeidaEn=ahora;
        }
        if(!mismoJugador||opcionesCambiadas||ahora-guardado.datosLentosLeidosEn>=200) {
            if (opciones&lecturas::Dinero) {
                const auto dinero=Leer<uintptr_t>(controlador+campos::servicioDinero);
                p.dinero=dinero ? Leer<int>(dinero+campos::cuentaDinero) : 0;
            }
            p.armadura=-1; p.casco=false; p.kit=false;
            if(opciones&lecturas::Armadura) {
                int valor=-1;
                if(LeerSeguro(personaje+campos::armadura,valor)&&valor>=0&&valor<=100) p.armadura=valor;
            }
            if(opciones&(lecturas::Casco|lecturas::Kit)) {
                const auto objetos=Leer<uintptr_t>(personaje+campos::servicioObjetos);
                struct Equipo { std::uint8_t kit=0,casco=0; } equipo;
                static_assert(campos::casco-campos::kit==offsetof(Equipo,casco));
                if(objetos&&LeerSeguro(objetos+campos::kit,equipo)) {
                    p.casco=(opciones&lecturas::Casco)&&equipo.casco==1;
                    p.kit=(opciones&lecturas::Kit)&&equipo.kit==1;
                }
            }
            guardado.datosLentosLeidosEn=ahora;
        }
        if ((opciones&lecturas::Portador)&&(!mismoJugador||opcionesCambiadas||ahora-guardado.inventarioLeidoEn>=250)) {
            p.llevaBomba=LlevaBomba(personaje,listaEntidades);
            guardado.inventarioLeidoEn=ahora;
        }

        // La pose se obtiene durante el dibujo, no en estos datos compartidos.
    }

    baseEspectadoresEn=ahora; baseEspectadoresLista=listaEntidades;
    std::lock_guard<std::mutex> bloqueo(mutexDatos);
    jugadorLocalGlobal = local;
    memcpy(jugadoresGlobales, jugadoresNuevos, sizeof(jugadoresNuevos));
    datosLeidosEn=GetTickCount64();
}

// Lectura de poses desde el hilo de dibujo.
bool LeerPoseJugador(DatosJugador& jugador,PoseJugador& pose) {

    struct EstadoPose { validacion::Reintento reintento; std::uint64_t usadoEn=0; };
    static std::array<EstadoPose,65> reintentos{};
    // Se usa exclusivamente desde el hilo de dibujo.
    const auto ahora=GetTickCount64();
    auto entrada=std::find_if(reintentos.begin(),reintentos.end(),[&](const EstadoPose& e) {
        return e.reintento.identidad==jugador.personaje;
    });
    if(entrada==reintentos.end()) entrada=std::min_element(reintentos.begin(),reintentos.end(),
        [](const EstadoPose& a,const EstadoPose& b) { return a.usadoEn<b.usadoEn; });
    entrada->usadoEn=ahora;
    auto& reintento=entrada->reintento;
    if(!reintento.Disponible(jugador.personaje,ahora)) { pose={}; return false; }
    bool correcto=false;
    struct Registrar { validacion::Reintento& r; bool& ok; uint64_t ahora; ~Registrar(){r.Resultado(ok,ahora);} } registrar{reintento,correcto,ahora};
    const auto escena=Leer<uintptr_t>(jugador.personaje+campos::nodoEscena);
    pose={};
    if (!escena) return false;
    Vector3 origen;
    if (!LeerSeguro(escena+campos::origenAbsoluto,origen)||
        !std::isfinite(origen.x)||!std::isfinite(origen.y)||!std::isfinite(origen.z)) return false;
    jugador.origen=origen;
    const auto direccion=Leer<uintptr_t>(escena+campos::estadoModelo+campos::matrizHuesos);
    struct DatosHueso { Vector3 posicion; float escala; float rotacion[4]; };
    static_assert(sizeof(DatosHueso)==32);
    std::array<DatosHueso,esqueleto::CantidadHuesos> huesos{};
    constexpr int cantidad=esqueleto::CantidadHuesos;
    if (!LeerBloque(direccion,huesos.data(),cantidad*sizeof(DatosHueso))) return false;
    for (int i=0;i<cantidad;++i) {
        const auto diferencia=huesos[i].posicion-origen;
        const float distanciaCuadrada=diferencia.x*diferencia.x+diferencia.y*diferencia.y+diferencia.z*diferencia.z;
        if (!huesos[i].posicion.EsCero()&&std::isfinite(distanciaCuadrada)&&distanciaCuadrada<14400.f)
        {
            pose.posicionesHuesos[i]=huesos[i].posicion;
        }
    }
    correcto=!pose.posicionesHuesos[CABEZA].EsCero(); return correcto;
}

static void ActualizarEspectadores() {

    std::vector<std::string> espectadores;
    const auto Publicar=[&]() { std::lock_guard<std::mutex> bloqueo(mutexDatos);
        espectadoresGlobales=std::move(espectadores); espectadoresLeidosEn=GetTickCount64(); };
    
    JugadorLocal local;
    {
        std::lock_guard<std::mutex> bloqueo(mutexDatos);
        local = jugadorLocalGlobal;
    }
    
    if (!local.personaje) { Publicar(); return; }
    
    uintptr_t listaEntidades = Leer<uintptr_t>(baseCliente + direcciones::globales::cliente::listaEntidades);
    if (!listaEntidades) { Publicar(); return; }
    
    uintptr_t controladorLocal = Leer<uintptr_t>(baseCliente + direcciones::globales::cliente::controladorLocal);
    uintptr_t personajeLocal = Leer<uintptr_t>(baseCliente + direcciones::globales::cliente::personajeLocal);
    
    uint32_t identificadorPersonajeLocal = 0;
    if (controladorLocal) {
        identificadorPersonajeLocal = Leer<uint32_t>(controladorLocal + campos::personaje);
    }
    
    const bool compartida=baseEspectadoresLista==listaEntidades&&MuestraReciente(baseEspectadoresEn,GetTickCount64(),100);
    const auto entrada=compartida ? 0 : Leer<uintptr_t>(listaEntidades+0x10);
    const auto separacion=SeparacionEntidades();
    std::array<std::byte,64*256> bloque{};
    const bool enBloque=entrada&&LeerBloque(entrada+separacion,bloque.data(),63*separacion+sizeof(uintptr_t));
    for (int i = 1; i <= 64; i++) {
        if (!compartida&&!entrada) continue;
        
        const auto& base=baseEspectadores[i];
        uintptr_t controlador=base.controlador;
        if(!compartida) { if(enBloque) std::memcpy(&controlador,bloque.data()+separacion*(i-1),sizeof(controlador)); else controlador=Leer<uintptr_t>(entrada+separacion*i); }
        if (!controlador || controlador == controladorLocal) continue;
        
        uint32_t identificadorPersonaje=compartida ? base.identificador : Leer<uint32_t>(controlador+campos::personaje);
        if (!identificadorPersonaje || identificadorPersonaje == 0xFFFFFFFF) continue;
        
        uintptr_t personaje=compartida ? base.personaje : ResolverEntidad(identificadorPersonaje,listaEntidades);
        if(!personaje||personaje==personajeLocal) continue;
        
        int salud=base.salud;
        uint8_t equipo=base.equipo;
        if(compartida ? !base.valida : (!LeerSeguro(personaje+campos::salud,salud)||!LeerSeguro(controlador+campos::equipo,equipo))) continue;
        if (salud > 0 && equipo != 1) continue;
        
        uintptr_t servicioObservacion = Leer<uintptr_t>(personaje + campos::servicioObservacion);
        if (!servicioObservacion) continue;
        
        uint32_t objetivoObservacion = Leer<uint32_t>(servicioObservacion + campos::objetivoObservacion);
        if (!objetivoObservacion || objetivoObservacion == 0xFFFFFFFF) continue;
        
        bool coincide = false;
        if (identificadorPersonajeLocal && objetivoObservacion == identificadorPersonajeLocal) coincide = true;
        
        if (coincide) {
            char nombre[128]{};
            const auto direccionNombre=Leer<uintptr_t>(controlador+campos::nombreJugador);
            if (LeerTexto(direccionNombre,nombre,sizeof(nombre))&&nombre[0]) espectadores.push_back(nombre);
        }
    }
    
    Publicar();
}

// El hilo actualiza la información; el dibujo lee la cámara y las poses actuales.
static void EjecutarHiloMemoria() {
    try {
    while (memoriaActiva.load()) {
        if (JuegoEnPausa()||!VisualesActivos()) {
            // Invalidar las muestras al pausar; al volver se comprueba la conexión.
            if(EstadoPublicado()!=EstadoPartida::Fuera) {
                PublicarEstadoPartida(EstadoPartida::Fuera); VaciarDatosJugadores(); sondeoPartida={};
            }
            std::unique_lock<std::mutex> bloqueo(mutexSuspension);
            despertarLector.wait(bloqueo,[] { return (!JuegoEnPausa()&&VisualesActivos())||!memoriaActiva.load(); });
            continue;
        }
        const auto comienzo=std::chrono::steady_clock::now();
        ActualizarJugadores();
        if(PartidaActiva()) { ActualizarDatosExtras(); mundo::Actualizar(); }
        static ULONGLONG ultimaLecturaEspectadores=0;
        const auto ahora=GetTickCount64();
        if (PartidaActiva()&&(LecturasActivas()&lecturas::Espectadores)) {
            if (ahora-ultimaLecturaEspectadores>=250) { ActualizarEspectadores(); ultimaLecturaEspectadores=ahora; }
        } else if (ultimaLecturaEspectadores) {
            std::lock_guard<std::mutex> bloqueo(mutexDatos);
            espectadoresGlobales.clear(); espectadoresLeidosEn=0; ultimaLecturaEspectadores=0;
        }
        const auto opciones=LecturasActivas();
        const auto intervalo=std::chrono::milliseconds(!PartidaActiva() ? (sondeoPartida.conectada ? 100 : 250) : !opciones ? 100 :
            (opciones&(lecturas::PosicionRapida|lecturas::ArmasSuelo|lecturas::GranadasMundo)) ? IntervaloLecturas() :
            (opciones&lecturas::Jugadores) ? 20 : 50);
        // El trabajo forma parte del intervalo; ceder al menos 1 ms tras un ciclo lento.
        std::unique_lock<std::mutex> bloqueo(mutexSuspension);
        despertarLector.wait_until(bloqueo,(std::max)(comienzo+intervalo,std::chrono::steady_clock::now()+std::chrono::milliseconds(1)),
            [] { return JuegoEnPausa()||!VisualesActivos()||!memoriaActiva.load(); });
    }
    } catch (...) {
        PublicarEstadoPartida(EstadoPartida::Desconocido);
        VaciarDatosJugadores();
        memoriaActiva = false;
    }
}

bool HiloMemoriaActivo() { return memoriaActiva.load(); }

// Iniciar hilo de lectura de memoria
void IniciarHiloMemoria() {
    std::lock_guard<std::mutex> bloqueo(mutexHilo);
    if (hiloMemoria.joinable()) return;
    VaciarDatosJugadores();
    sondeoPartida={}; PublicarEstadoPartida(EstadoPartida::Fuera);
    memoriaActiva = true;
    try { hiloMemoria = std::thread(EjecutarHiloMemoria); }
    catch (...) { memoriaActiva = false; throw; }
}

// Detener hilo de lectura de memoria
void DetenerHiloMemoria() {
    std::lock_guard<std::mutex> bloqueo(mutexHilo);
    {
        std::lock_guard<std::mutex> suspension(mutexSuspension);
        memoriaActiva = false;
    }
    despertarLector.notify_all();
    if (hiloMemoria.joinable()) hiloMemoria.join();
    VaciarDatosJugadores();
    sondeoPartida={}; PublicarEstadoPartida(EstadoPartida::Fuera);
}

