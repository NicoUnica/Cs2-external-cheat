#include "datos.hpp"
#include "juego.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <span>

namespace {
namespace danio {
// Estructura del registro de daño (CDamageRecord): https://github.com/a2x/cs2-dumper/blob/main/output/client_dll.hpp
struct Registro {
    std::byte base[0x30]{};
    std::uint32_t atacante=0,receptor=0,controladorAtacante=0,controladorReceptor=0;
    std::uintptr_t nombreAtacante=0,nombreReceptor=0;
    std::uint64_t xuidAtacante=0,xuidReceptor=0;
    float balas=0,total=0,salud=0;
    int impactos=0,ultimaBala=0;
    std::byte final[4]{};
};
static_assert(sizeof(Registro)==0x78&&offsetof(Registro,total)==0x64&&offsetof(Registro,salud)==0x68);
inline bool Valido(const Registro& r) {
    return std::isfinite(r.total)&&std::isfinite(r.salud)&&std::isfinite(r.balas)&&r.total>=0&&r.total<=100000&&
        r.salud>=0&&r.salud<=100000&&r.balas>=0&&r.balas<=100000&&r.impactos>=0&&r.impactos<=100000;
}
class Seguimiento {
    struct Acumulado {
        std::uint32_t controladorReceptor=0,receptor=0;
        std::uint64_t xuidReceptor=0;
        float total=0,salud=0;
    };
    std::array<Acumulado,128> anteriores{};
    std::size_t cantidad=0;
    std::uintptr_t propietario=0,servicio=0;
    bool iniciado=false;
public:
    float Actualizar(std::uintptr_t actual,std::uintptr_t fuente,std::span<const Registro> registros,bool nuevaRonda=false) {
        float incremento=0;
        if(actual!=propietario||fuente!=servicio||nuevaRonda) { *this={}; propietario=actual; servicio=fuente; }
        if(!actual||!fuente||registros.size()>anteriores.size()) return incremento;
        for(const auto& r:registros) if(!Valido(r)) return incremento;
        for(std::size_t i=0;i<registros.size();++i) {
            const auto& r=registros[i];
            auto fin=anteriores.begin()+cantidad;
            auto previo=std::find_if(anteriores.begin(),fin,[&](const auto& p) {
                return p.controladorReceptor==r.controladorReceptor&&p.receptor==r.receptor&&p.xuidReceptor==r.xuidReceptor;
            });
            float total=0,salud=0;
            const Acumulado actual{r.controladorReceptor,r.receptor,r.xuidReceptor,r.total,r.salud};
            if(previo!=fin) {
                // Una muestra anterior no puede rebajar lo ya registrado y repetirlo al llegar de nuevo.
                if(r.total<previo->total||r.salud<previo->salud) continue;
                total=r.total-previo->total; salud=r.salud-previo->salud;
                *previo=actual;
            } else if(cantidad<anteriores.size()) { anteriores[cantidad++]=actual; if(iniciado) {total=r.total;salud=r.salud;} }
            if(iniciado&&(total>.01f||salud>.01f)) incremento+=(std::max)(total,salud);
        }
        iniciado=true;
        return incremento;
    }
};
struct ContadorTotal {
    std::uintptr_t propietario=0,servicio=0;
    float anterior=0;
    bool iniciado=false;
    float Actualizar(std::uintptr_t actual,std::uintptr_t fuente,float total,bool& reinicio) {
        reinicio=false;
        if(!actual||!fuente||!std::isfinite(total)||total<0||total>1000000) { *this={};return 0; }
        const bool mismo=iniciado&&propietario==actual&&servicio==fuente;
        reinicio=mismo&&total+.01f<anterior;
        const float incremento=mismo&&!reinicio ? (std::max)(0.f,total-anterior) : 0.f;
        propietario=actual; servicio=fuente; anterior=total; iniciado=true; return incremento;
    }
};
// Ambos contadores pueden llegar en cuadros distintos: acreditar lo ya mostrado una sola vez.
class Conciliacion {
    float pendientes=0,credito=0;
public:
    bool Actualizar(float total,float registros) {
        if(!std::isfinite(total)||!std::isfinite(registros)||total<0||registros<0) return false;
        const float aclarado=(std::min)(pendientes,registros);
        pendientes-=aclarado;
        const float nuevoDetalle=registros-aclarado;
        credito+=nuevoDetalle;
        const float acreditado=(std::min)(credito,total);
        credito-=acreditado;
        const float sinDesglose=total-acreditado;
        pendientes+=sinDesglose;
        return sinDesglose>.01f||nuevoDetalle>.01f;
    }
};
}


std::mutex mutex;
extras::Instantanea instantanea;
danio::Seguimiento seguimientoDanio;
danio::ContadorTotal contadorDanio;
danio::Conciliacion conciliacionDanio;
uintptr_t propietarioDanio=0;
std::uint64_t ultimaActualizacion=0;

bool LeerRegistrosDanio(const JugadorLocal& local,std::array<danio::Registro,128>& propios,std::size_t& cantidad,uintptr_t& servicio) {
    cantidad=0; servicio=Leer<uintptr_t>(local.controlador+campos::servicioDanio);
    struct Vector { int cantidad=0,relleno=0; uintptr_t datos=0; } vector;
    int actualizacion=0,comprobacion=0;
    if(!servicio||!LeerSeguro(servicio+campos::actualizacionDanio,actualizacion)||
       !LeerSeguro(servicio+campos::listaDanio,vector)||vector.cantidad<0||vector.cantidad>128) return false;
    std::array<danio::Registro,128> registros{};
    if(vector.cantidad&&(!vector.datos||!LeerBloque(vector.datos,registros.data(),vector.cantidad*sizeof(danio::Registro)))) return false;
    if(!LeerSeguro(servicio+campos::actualizacionDanio,comprobacion)||actualizacion!=comprobacion) return false;
    const auto lista=Leer<uintptr_t>(baseCliente+direcciones::globales::cliente::listaEntidades);
    if(!lista) return false;
    std::uint32_t atacanteLocal=0;
    for(int i=0;i<vector.cantidad;++i) {
        const auto& r=registros[i];
        if(!danio::Valido(r)) return false;
        if(!r.controladorAtacante||r.controladorAtacante==0xFFFFFFFF||r.controladorAtacante==r.controladorReceptor) continue;
        if(r.controladorAtacante!=atacanteLocal) {
            const auto indice=r.controladorAtacante&0x7FFF;
            const auto pagina=Leer<uintptr_t>(lista+0x10+8*(indice>>9));
            if(!pagina||Leer<uintptr_t>(pagina+SeparacionEntidades()*(indice&0x1FF))!=local.controlador) continue;
            atacanteLocal=r.controladorAtacante;
        }
        propios[cantidad++]=r;
    }
    return true;
}

void ActualizarDanio(const JugadorLocal& local,std::uint32_t opciones,std::uint64_t ahora,extras::Instantanea& siguiente) {
    if(!(opciones&lecturas::Impactos)||!local.controlador) {
        seguimientoDanio={}; contadorDanio={}; conciliacionDanio={}; propietarioDanio=0;
        siguiente.impactoEn=0; return;
    }
    if(propietarioDanio!=local.controlador) {
        seguimientoDanio={}; contadorDanio={}; conciliacionDanio={};
        siguiente.impactoEn=0; propietarioDanio=local.controlador;
    }
    const auto servicioTotal=Leer<uintptr_t>(local.controlador+campos::servicioEstadisticas);
    float total=0; bool reinicio=false;
    const bool totalValido=servicioTotal&&LeerSeguro(servicioTotal+campos::totalDanioRonda,total)&&std::isfinite(total)&&total>=0&&total<=1000000;
    const float incremento=totalValido ? contadorDanio.Actualizar(local.controlador,servicioTotal,total,reinicio) : 0.f;
    if(!totalValido) contadorDanio={};
    if(reinicio) { conciliacionDanio={}; }
    std::array<danio::Registro,128> registros{}; std::size_t cantidad=0; uintptr_t servicio=0;
    const bool registrosValidos=LeerRegistrosDanio(local,registros,cantidad,servicio);
    float incrementoRegistros=0;
    if(registrosValidos)
        incrementoRegistros=seguimientoDanio.Actualizar(local.controlador,servicio,std::span(registros.data(),cantidad),reinicio);
    else if(reinicio) seguimientoDanio={};
    if(conciliacionDanio.Actualizar(incremento,incrementoRegistros)) siguiente.impactoEn=ahora;
}
bool LeerBombaPlantada(uintptr_t entidad,float tiempoJuego,bool tiempoValido,extras::EstadoBomba& bomba) {
    std::uint8_t activa=0,desactivada=0,desactivando=0; int zona=-1; float fin=0,duracion=0;
    if (!entidad||!LeerSeguro(entidad+campos::bombaActiva,activa)||activa!=1||
        !LeerSeguro(entidad+campos::bombaDesactivada,desactivada)||desactivada!=0||
        !LeerSeguro(entidad+campos::zonaBomba,zona)||zona<0||zona>1||
        !LeerSeguro(entidad+campos::duracionTemporizador,duracion)||!std::isfinite(duracion)||duracion<10||duracion>120||
        !LeerSeguro(entidad+campos::instanteExplosion,fin)||!std::isfinite(fin)||!LeerOrigenMundo(entidad,bomba.origen)) return false;
    bomba.tiempoValido=tiempoValido&&extras::CuentaAtrasBomba(fin,tiempoJuego,duracion,bomba.restante);
    if (tiempoValido&&!bomba.tiempoValido) return false;
    bomba.visible=true; bomba.plantada=true; bomba.zona=zona; bomba.duracion=duracion;
    float finDesactivacion=0;
    if (bomba.tiempoValido&&LeerSeguro(entidad+campos::desactivacionActiva,desactivando)&&desactivando==1&&
        LeerSeguro(entidad+campos::instanteDesactivacion,finDesactivacion)&&std::isfinite(finDesactivacion)&&
        finDesactivacion>tiempoJuego&&finDesactivacion-tiempoJuego<=12.f) {
        bomba.desactivando=true; bomba.desactivacionRestante=finDesactivacion-tiempoJuego;
    }
    return true;
}
extras::EstadoBomba LeerBomba() {
    extras::EstadoBomba bomba{};
    const auto variablesGlobales=Leer<uintptr_t>(baseCliente+direcciones::globales::cliente::variablesGlobales);
    float tiempoJuego=0;
    const bool tiempoValido=variablesGlobales&&LeerSeguro(variablesGlobales+campos::tiempoJuego,tiempoJuego)&&std::isfinite(tiempoJuego)&&tiempoJuego>=0&&tiempoJuego<10000000.f;
    const auto plantada=Leer<uintptr_t>(baseCliente+direcciones::globales::cliente::bombaPlantada);
    // Admitir una dirección directa o un contenedor de un nivel y validar ambas lecturas.
    if (LeerBombaPlantada(plantada,tiempoJuego,tiempoValido,bomba)) return bomba;
    bomba={};
    if (plantada&&LeerBombaPlantada(Leer<uintptr_t>(plantada),tiempoJuego,tiempoValido,bomba)) return bomba;
    const auto arma=Leer<uintptr_t>(baseCliente+direcciones::globales::cliente::armaBomba);
    for (const auto entidad : {arma,arma ? Leer<uintptr_t>(arma) : uintptr_t{0}}) {
        std::uint16_t identificador=0; std::uint32_t propietario=0;
        if (entidad&&LeerSeguro(entidad+campos::atributos+campos::objeto+campos::definicionObjeto,identificador)&&identificador==49&&
            LeerSeguro(entidad+campos::propietario,propietario)&&(propietario==0||propietario==0xFFFFFFFF)) {
            bomba={};
            if (LeerOrigenMundo(entidad,bomba.origen)) { bomba.visible=true; return bomba; }
        }
    }
    return {};
}

}

void ReiniciarDatosExtras() {
    std::lock_guard<std::mutex> bloqueo(mutex);
    instantanea={}; seguimientoDanio={}; contadorDanio={}; conciliacionDanio={}; propietarioDanio=0; ultimaActualizacion=0;
}
void ActualizarDatosExtras() {
    const auto opciones=LecturasActivas();
    if(!(opciones&(lecturas::Impactos|lecturas::Bomba))) {
        if(ultimaActualizacion) ReiniciarDatosExtras();
        return;
    }
    const auto ahora=GetTickCount64();
    if (ahora-ultimaActualizacion<20) return;
    ultimaActualizacion=ahora;
    JugadorLocal local{};
    if(opciones&lecturas::Impactos) CopiarDatosJuego(local);
    extras::Instantanea siguiente;
    { std::lock_guard<std::mutex> bloqueo(mutex); siguiente=instantanea; }
    siguiente.leidoEn=ahora;
    ActualizarDanio(local,opciones,ahora,siguiente);
    if(opciones&lecturas::Bomba) {
        siguiente.bomba=LeerBomba(); siguiente.bombaLeidaEn=ahora;
    } else { siguiente.bomba={}; siguiente.bombaLeidaEn=0; }
    { std::lock_guard<std::mutex> bloqueo(mutex); instantanea=std::move(siguiente); }
}
extras::Instantanea CopiarDatosExtras() {
    std::lock_guard<std::mutex> bloqueo(mutex);
    return instantanea;
}

namespace mundo {
namespace {
struct RelojTicks {
    int tickAnterior=0;
    float tiempoAnterior=0,intervalo=0;
    float Actualizar(int tick,float tiempo) {
        if(tick<=0||!std::isfinite(tiempo)||tiempo<0) { *this={}; return 0; }
        if(!tickAnterior||tick<tickAnterior||tiempo<tiempoAnterior) {
            tickAnterior=tick; tiempoAnterior=tiempo; intervalo=0; return 0;
        }
        const auto diferencia=tiempo-tiempoAnterior;
        if(diferencia>=.5f&&tick>tickAnterior) {
            const float candidato=diferencia/float(tick-tickAnterior);
            intervalo=std::isfinite(candidato)&&candidato>=.004f&&candidato<=.05f ? candidato : 0;
            tickAnterior=tick; tiempoAnterior=tiempo;
        }
        return intervalo;
    }
};

struct Entrada {
    uintptr_t entidad=0;
    Tipo tipo=Tipo::Ninguno;
    std::uint64_t clasificadaEn=0;
    std::uint16_t id=0;
    std::array<char,64> nombre{};
};
std::atomic<std::shared_ptr<const Cuadro>> publicado;
std::array<std::shared_ptr<Cuadro>,3> cuadros{};
std::vector<Entrada> directorio;
std::vector<std::size_t> candidatos;
std::vector<std::byte> bloque;
std::uint64_t descubiertoEn=0,actualizadoEn=0;
uintptr_t listaAnterior=0;
std::uint32_t opcionesAnteriores=0;
std::size_t paginaDescubrimiento=0;
RelojTicks relojTicks;
constexpr auto OpcionesMundo=lecturas::ArmasSuelo|lecturas::GranadasMundo|lecturas::TiempoGranadas;

const char* NombreArmaSuelo(std::uint16_t id) {
    // La C4 se consulta por separado, también cuando está en el suelo.
    if(id==49) return nullptr;
    const auto* dato=armas::Buscar(id); return dato ? dato->nombre : nullptr;
}
Tipo Clasificar(const char* nombre) {
    if(!std::strncmp(nombre,"weapon_",7)) return Tipo::Arma;
    if(!std::strcmp(nombre,"hegrenade_projectile")) return Tipo::HE;
    if(!std::strcmp(nombre,"flashbang_projectile")) return Tipo::Flash;
    if(!std::strcmp(nombre,"smokegrenade_projectile")) return Tipo::Humo;
    if(!std::strcmp(nombre,"molotov_projectile")||!std::strcmp(nombre,"incgrenade_projectile")) return Tipo::Molotov;
    if(!std::strcmp(nombre,"decoy_projectile")) return Tipo::Senuelo;
    if(!std::strcmp(nombre,"inferno")) return Tipo::Fuego;
    return Tipo::Ninguno;
}
void Descubrir(uintptr_t lista,std::uint64_t ahora,std::uint32_t opciones) {
    // El campo anunciado como highestEntityIndex puede contener el recuento de entidades.
    // Recorrer los bloques presentes evita perder índices altos cuando hay huecos.
    std::array<uintptr_t,64> paginas{};
    if(!LeerBloque(lista+0x10,paginas.data(),sizeof(paginas))) return;
    const auto paso=SeparacionEntidades();
    if(paso<sizeof(uintptr_t)||paso>256) return;
    std::size_t cantidadPaginas=0;
    for(std::size_t i=0;i<paginas.size();++i) {
        if(paginas[i]<0x10000||paginas[i]>=0x0000800000000000ULL||paginas[i]%alignof(uintptr_t)) paginas[i]=0;
        if(paginas[i]) cantidadPaginas=i+1;
    }
    directorio.resize(cantidadPaginas*512);
    bloque.resize(511*paso+sizeof(uintptr_t));
    const auto comienzo=(std::min)(paginaDescubrimiento,cantidadPaginas);
    const auto fin=(std::min)(comienzo+16,cantidadPaginas);
    std::erase_if(candidatos,[&](std::size_t i){return i>=directorio.size()||(i/512>=comienzo&&i/512<fin);});
    for(std::size_t pagina=comienzo;pagina<fin;++pagina) {
        const bool valido=paginas[pagina]&&LeerBloque(paginas[pagina],bloque.data(),bloque.size());
        for(std::size_t i=0;i<512;++i) {
            auto& entrada=directorio[pagina*512+i];
            if(paginas[pagina]&&!valido) continue; // Reintentar una lectura transitoria sin vaciar el bloque.
            uintptr_t entidad=0;
            if(valido) std::memcpy(&entidad,bloque.data()+i*paso,sizeof(entidad));
            if(!entidad) { entrada={}; continue; }
            const auto intervalo=entrada.tipo==Tipo::Arma&&!entrada.id ? 250u : 2000u;
            if(entrada.entidad==entidad&&entrada.clasificadaEn&&ahora-entrada.clasificadaEn<intervalo) continue;
            entrada={}; entrada.entidad=entidad;
            const auto identidad=Leer<uintptr_t>(entidad+campos::identidad);
            const auto nombre=identidad ? Leer<uintptr_t>(identidad+campos::nombreInterno) : 0;
            char texto[128]{};
            if(!nombre||!LeerTexto(nombre,texto,sizeof(texto))) continue;
            entrada.clasificadaEn=ahora; entrada.tipo=Clasificar(texto);
            if(entrada.tipo==Tipo::Arma) {
                if(!(opciones&lecturas::ArmasSuelo)) { entrada.tipo=Tipo::Ninguno; continue; }
                LeerSeguro(entidad+campos::atributos+campos::objeto+campos::definicionObjeto,entrada.id);
                if(entrada.id==49||!std::strcmp(texto,"weapon_c4")) { entrada.tipo=Tipo::Ninguno; continue; }
                if(const auto etiqueta=NombreArmaSuelo(entrada.id)) strncpy_s(entrada.nombre.data(),entrada.nombre.size(),etiqueta,_TRUNCATE);
                else {
                    // Seguir mostrando armas nuevas o sin definición leída, con un nombre reconocible.
                    strncpy_s(entrada.nombre.data(),entrada.nombre.size(),texto+7,_TRUNCATE);
                    for(auto& letra:entrada.nombre) if(letra=='_') letra=' ';
                    if(entrada.nombre[0]>='a'&&entrada.nombre[0]<='z') entrada.nombre[0]-='a'-'A';
                }
            }

        }
    }
    for(std::size_t i=comienzo*512;i<fin*512;++i) if(directorio[i].tipo!=Tipo::Ninguno) candidatos.push_back(i);
    paginaDescubrimiento=fin>=cantidadPaginas ? 0 : fin;
}
bool LeerFuego(const Entrada& entrada,Objeto& objeto) {
    int cantidad=0; std::uint8_t terminado=0;
    return LeerSeguro(entrada.entidad+campos::cantidadFuego,cantidad)&&cantidad>0&&cantidad<=64&&
        LeerSeguro(entrada.entidad+campos::fuegoTerminado,terminado)&&terminado==0&&LeerOrigenMundo(entrada.entidad,objeto.origen);
}


}
void Reiniciar() {
    publicado.store(nullptr); cuadros={}; directorio.clear(); candidatos.clear(); bloque.clear();
    descubiertoEn=actualizadoEn=0; listaAnterior=0; opcionesAnteriores=0; paginaDescubrimiento=0;
    relojTicks={};
}
void Actualizar() {
    const auto opciones=LecturasActivas()&OpcionesMundo;
    if(!opciones) { if(opcionesAnteriores) Reiniciar(); return; }
    const auto ahora=GetTickCount64();
    const bool cambio=opciones!=opcionesAnteriores;
    if(!cambio&&ahora-actualizadoEn<16) return;
    actualizadoEn=ahora;
    if(cambio) for(auto& entrada:directorio) entrada.clasificadaEn=0;
    opcionesAnteriores=opciones;
    const auto lista=Leer<uintptr_t>(baseCliente+direcciones::globales::cliente::listaEntidades);
    if(!lista) { Reiniciar(); return; }
    if(lista!=listaAnterior) { directorio.clear(); candidatos.clear(); descubiertoEn=0; listaAnterior=lista; paginaDescubrimiento=0; }
    if(cambio) paginaDescubrimiento=0;
    if(cambio||!descubiertoEn||ahora-descubiertoEn>=(directorio.size()>16*512 ? 32u : 125u)) { Descubrir(lista,ahora,opciones); descubiertoEn=ahora; }
    std::shared_ptr<Cuadro> cuadro;
    for(auto& disponible:cuadros) {
        if(!disponible) { disponible=std::make_shared<Cuadro>(); disponible->objetos.reserve(256); }
        if(disponible.use_count()==1) { cuadro=disponible; break; }
    }
    if(!cuadro) return;
    cuadro->leidoEn=ahora; cuadro->objetos.clear();
    const auto variables=(opciones&lecturas::TiempoGranadas) ? Leer<uintptr_t>(baseCliente+direcciones::globales::cliente::variablesGlobales) : 0;
    float tiempo=0;
    const bool reloj=variables&&LeerSeguro(variables+campos::tiempoJuego,tiempo)&&std::isfinite(tiempo)&&tiempo>=0&&tiempo<10000000;
    const int tickActual=reloj ? TickJuego() : 0;
    const float intervaloTick=relojTicks.Actualizar(tickActual,tiempo);
    JugadorLocal local{}; CopiarDatosJuego(local);
    const float distanciaMaxima=DistanciaObjetosActiva()*39.37f;
    for(const auto indice:candidatos) {
        auto& entrada=directorio[indice];
        const bool arma=entrada.tipo==Tipo::Arma;
        if(arma ? !(opciones&lecturas::ArmasSuelo) : !(opciones&lecturas::GranadasMundo)) continue;
        Objeto objeto; objeto.tipo=entrada.tipo;
        if(arma) {
            std::uint32_t propietario=0;
            if(!LeerSeguro(entrada.entidad+campos::propietario,propietario)||(propietario&&propietario!=0xFFFFFFFF)) continue;
            objeto.nombre=entrada.nombre;
            if(!LeerOrigenMundo(entrada.entidad,objeto.origen)) continue;
            if(local.vistaValida&&optimizacion::DistanciaCuadrada(local.origenVista,objeto.origen)>distanciaMaxima*distanciaMaxima) continue;
        } else if(entrada.tipo==Tipo::Fuego) {
            if(!LeerFuego(entrada,objeto)) continue;
        } else {
            if(!LeerOrigenMundo(entrada.entidad,objeto.origen)) continue;
            if(entrada.tipo!=Tipo::Humo) {
                std::uint8_t explotada=0;
                if(!LeerSeguro(entrada.entidad+campos::explosionIniciada,explotada)||explotada>1||explotada) continue;
            }
        }
        if(entrada.tipo==Tipo::Humo) {
            Vector3 origen{};
            if(LeerSeguro(entrada.entidad+campos::origenHumo,origen)&&PosicionMundoValida(origen)) objeto.origen=origen;
        }
        const float distancia=local.vistaValida ? optimizacion::DistanciaCuadrada(local.origenVista,objeto.origen) : 0.f;
        if(local.vistaValida&&distancia>distanciaMaxima*distanciaMaxima) continue;
        if(!arma&&reloj&&(opciones&lecturas::TiempoGranadas)) {
            int tick=0; float duracion=0;
            if(entrada.tipo==Tipo::Humo) {
                std::uint8_t activa=0;
                if(LeerSeguro(entrada.entidad+campos::humoActivo,activa)&&activa==1&&LeerSeguro(entrada.entidad+campos::tickHumo,tick)) duracion=18.f;
            } else if(entrada.tipo==Tipo::Fuego&&LeerSeguro(entrada.entidad+campos::tickFuego,tick)) {
                duracion=7.f;
                float leida=0;
                if(LeerSeguro(entrada.entidad+campos::duracionFuego,leida)&&std::isfinite(leida)&&leida>=1&&leida<=30) duracion=leida;
            }
            if(duracion>0&&tick>0&&intervaloTick>0) {
                const float restante=float(tick-tickActual)*intervaloTick+duracion;
                if(restante<=0) continue;
                if(restante<=duracion+.5f) objeto.restante=restante;
            }
        }
        cuadro->objetos.push_back(std::move(objeto));
    }
    publicado.store(std::move(cuadro));
}
std::shared_ptr<const Cuadro> CopiarDatos() {
    return publicado.load();
}
}
