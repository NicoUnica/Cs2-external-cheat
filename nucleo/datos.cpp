#include "datos.hpp"
#include "juego.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace {
std::mutex mutex;
extras::Instantanea instantanea;
extras::ContadorImpactos impactos;
std::uint64_t ultimaActualizacion=0;

bool LeerOrigenMundo(uintptr_t entidad,Vector3& origen) {
    const auto escena=Leer<uintptr_t>(entidad+campos::nodoEscena);
    return escena&&LeerSeguro(escena+campos::origenAbsoluto,origen)&&extras::EsFinito(origen)&&
        std::abs(origen.x)<100000.f&&std::abs(origen.y)<100000.f&&std::abs(origen.z)<100000.f&&!origen.EsCero();
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
    instantanea={}; impactos={}; ultimaActualizacion=0;
}
void ActualizarDatosExtras() {
    const auto ahora=GetTickCount64();
    if (ahora-ultimaActualizacion<20) return;
    ultimaActualizacion=ahora;
    const auto opciones=LecturasActivas();
    if(!(opciones&(lecturas::Impactos|lecturas::Bomba))) return;
    JugadorLocal local{};
    if(opciones&lecturas::Impactos) CopiarDatosJuego(local);
    extras::Instantanea siguiente;
    { std::lock_guard<std::mutex> bloqueo(mutex); siguiente=instantanea; }
    siguiente.leidoEn=ahora;
    int cantidad=0; uintptr_t servicio=0;
    if ((opciones&lecturas::Impactos)&&local.esValido&&LeerSeguro(local.personaje+campos::servicioBalas,servicio)&&servicio&&
        LeerSeguro(servicio+campos::impactosServidor,cantidad)&&cantidad>=0&&cantidad<100000) {
        if (impactos.Actualizar(local.personaje,servicio,cantidad)) siguiente.impactoEn=ahora;
    } else { impactos={}; siguiente.impactoEn=0; }
    if(opciones&lecturas::Bomba) {
        if(!siguiente.bombaLeidaEn||ahora-siguiente.bombaLeidaEn>=20) {
            siguiente.bomba=LeerBomba(); siguiente.bombaLeidaEn=ahora;
        }
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
    Vector3 ultimoOrigen{};
    std::uint64_t origenLeidoEn=0;
    int balasCargador=-1,balasReserva=-1;
    std::uint64_t municionLeidaEn=0;
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

bool PosicionValida(Vector3 p) {
    return extras::EsFinito(p)&&!p.EsCero()&&std::abs(p.x)<100000&&std::abs(p.y)<100000&&std::abs(p.z)<100000;
}
bool Origen(uintptr_t entidad,Vector3& salida) {
    const auto escena=Leer<uintptr_t>(entidad+campos::nodoEscena);
    return escena&&LeerSeguro(escena+campos::origenAbsoluto,salida)&&PosicionValida(salida);
}
const char* NombreArmaSuelo(std::uint16_t id) {
    // La C4 se entrega a través de la instantánea de bomba.
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
            const auto anterior=entrada;
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
            if(entrada.entidad==anterior.entidad&&entrada.tipo==anterior.tipo&&entrada.id==anterior.id) {
                entrada.ultimoOrigen=anterior.ultimoOrigen; entrada.origenLeidoEn=anterior.origenLeidoEn;
                entrada.balasCargador=anterior.balasCargador;
                entrada.balasReserva=anterior.balasReserva;
                entrada.municionLeidaEn=anterior.municionLeidaEn;
            }
        }
    }
    for(std::size_t i=comienzo*512;i<fin*512;++i) if(directorio[i].tipo!=Tipo::Ninguno) candidatos.push_back(i);
    paginaDescubrimiento=fin>=cantidadPaginas ? 0 : fin;
}
bool LeerFuego(const Entrada& entrada,Objeto& objeto) {
    int cantidad=0; std::uint8_t terminado=0;
    return LeerSeguro(entrada.entidad+campos::cantidadFuego,cantidad)&&cantidad>0&&cantidad<=64&&
        LeerSeguro(entrada.entidad+campos::fuegoTerminado,terminado)&&terminado==0&&Origen(entrada.entidad,objeto.origen);
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
            if(!Origen(entrada.entidad,objeto.origen)) continue;
            // No consultar munición de objetos que no se van a mostrar.
            if(local.vistaValida&&optimizacion::DistanciaCuadrada(local.origenVista,objeto.origen)>distanciaMaxima*distanciaMaxima) continue;
            const auto* dato=armas::Buscar(entrada.id);
            if(!dato||dato->recarga) {
                if(!entrada.municionLeidaEn||ahora-entrada.municionLeidaEn>=200) {
                    int cargador=-1,reserva=-1;
                    const bool valida=LeerSeguro(entrada.entidad+campos::balasCargador,cargador)&&
                        LeerSeguro(entrada.entidad+campos::balasReserva,reserva)&&
                        cargador>=0&&cargador<=250&&reserva>=0&&reserva<=1000;
                    entrada.balasCargador=valida ? cargador : -1;
                    entrada.balasReserva=valida ? reserva : -1;
                    entrada.municionLeidaEn=ahora;
                }
                objeto.balasCargador=entrada.balasCargador;
                objeto.balasReserva=entrada.balasReserva;
            }
        } else if(entrada.tipo==Tipo::Fuego) {
            if(!LeerFuego(entrada,objeto)) continue;
        } else {
            if(!Origen(entrada.entidad,objeto.origen)) continue;
            if(entrada.tipo!=Tipo::Humo) {
                std::uint8_t explotada=0;
                if(!LeerSeguro(entrada.entidad+campos::explosionIniciada,explotada)||explotada>1||explotada) continue;
            }
        }
        if(entrada.tipo==Tipo::Humo) {
            Vector3 origen{};
            if(LeerSeguro(entrada.entidad+campos::origenHumo,origen)&&PosicionValida(origen)) objeto.origen=origen;
        }
        // Estimar movimiento con muestras recientes; no predecir saltos o cambios de entidad.
        if(entrada.origenLeidoEn&&ahora>entrada.origenLeidoEn&&ahora-entrada.origenLeidoEn<=100) {
            const auto diferencia=objeto.origen-entrada.ultimoOrigen;
            const float segundos=static_cast<float>(ahora-entrada.origenLeidoEn)*.001f;
            const float desplazamiento=optimizacion::DistanciaCuadrada(objeto.origen,entrada.ultimoOrigen);
            if(desplazamiento<=3500.f*3500.f*segundos*segundos) objeto.velocidad=diferencia*(1.f/segundos);
        }
        entrada.ultimoOrigen=objeto.origen; entrada.origenLeidoEn=ahora;
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
