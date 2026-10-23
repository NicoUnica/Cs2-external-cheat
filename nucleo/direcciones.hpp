#pragma once
#include <cstdint>
// Volcado local de cs2-dumper: compilación 14192, 2026-10-23.
namespace direcciones::globales::cliente {
inline constexpr uintptr_t listaEntidades=0x271C8B8;
inline constexpr uintptr_t controladorLocal=0x253EEF8;
inline constexpr uintptr_t personajeLocal=0x25676D8;
inline constexpr uintptr_t matrizVista=0x256CFE0;
inline constexpr uintptr_t bombaPlantada=0x24CD930;
inline constexpr uintptr_t armaBomba=0x24CBEE8;
inline constexpr uintptr_t variablesGlobales=0x2232088;
}
namespace direcciones::globales::motor {
// Procedencia: offsets.hpp del mismo volcado local, compilación 14192.
inline constexpr int compilacionEsperada=14192;
inline constexpr uintptr_t compilacion=0x6BA02C;
inline constexpr uintptr_t clienteRed=0x9B88C0;
inline constexpr uintptr_t tickCliente=0x398;
inline constexpr uintptr_t estadoConexion=0x230;
inline constexpr uintptr_t mapaFondo=0x2C143F;
}

// Campos contrastados con client_dll.json, 2026-10-23T07:59:08.410824600+00:00.
namespace campos {
constexpr uintptr_t identidad=0x10; // Campo del esquema: CEntityInstance
constexpr uintptr_t nombreInterno=0x20; // Campo del esquema: CEntityIdentity

constexpr uintptr_t nodoEscena=0x330;
constexpr uintptr_t salud=0x34C;
constexpr uintptr_t estadoVida=0x354;
constexpr uintptr_t equipo=0x3E7;

constexpr uintptr_t servicioArmas=0x12F0;
constexpr uintptr_t origenAnterior=0x14A4;

constexpr uintptr_t tiempoCeguera=0x14FC; // Campo del esquema: C_CSPlayerPawnBase::m_flFlashBangTime
constexpr uintptr_t servicioObjetos=0x12F8; // Campo del esquema: C_BasePlayerPawn::m_pItemServices
constexpr uintptr_t armadura=0x1ED0; // Campo del esquema: C_CSPlayerPawn::m_ArmorValue
constexpr uintptr_t casco=0x49,kit=0x48; // Campo del esquema: CCSPlayer_ItemServices
constexpr uintptr_t desactivando=0x1EA2,inmune=0x3508; // Campo del esquema: C_CSPlayerPawn
constexpr uintptr_t recargando=0x1A3C; // Campo del esquema: C_CSWeaponBase::m_bInReload
constexpr uintptr_t balasCargador=0x1928; // Campo del esquema: C_BasePlayerWeapon::m_iClip1
constexpr uintptr_t balasReserva=0x1930; // Campo del esquema: C_BasePlayerWeapon::m_pReserveAmmo[0]

constexpr uintptr_t origenAbsoluto=0xC8;

constexpr uintptr_t estadoModelo=0x140;

constexpr uintptr_t matrizHuesos = 0x80; // No incluido en el volcado; valor previo sin verificar.

constexpr uintptr_t armaActiva=0x60;

constexpr uintptr_t personaje=0x6BC;
constexpr uintptr_t nombreJugador=0x878;
constexpr uintptr_t servicioDinero=0x818;
constexpr uintptr_t cuentaDinero=0x40;

constexpr uintptr_t personajeObservador=0x930;
constexpr uintptr_t personajeVivo=0x934;
constexpr uintptr_t modoObservacion=0x48;
constexpr uintptr_t servicioObservacion=0x1308;
constexpr uintptr_t objetivoObservacion=0x4C;

constexpr uintptr_t apuntando=0x1EA0;
constexpr uintptr_t inventarioArmas=0x48;
constexpr uintptr_t atributos=0x1290;
constexpr uintptr_t objeto=0x50;
constexpr uintptr_t definicionObjeto=0x1BA;
// Esquema local de cs2-dumper, 2026-10-23, compilación 14192.
constexpr uintptr_t servicioDanio=0x830,servicioEstadisticas=0x828;
constexpr uintptr_t listaDanio=0x48,actualizacionDanio=0x40,totalDanioRonda=0x130;
constexpr uintptr_t propietario=0x520;
constexpr uintptr_t bombaActiva=0x1288;
constexpr uintptr_t zonaBomba=0x128C;
constexpr uintptr_t instanteExplosion=0x12B8;
constexpr uintptr_t duracionTemporizador=0x12C0;
constexpr uintptr_t desactivacionActiva=0x12C4;
constexpr uintptr_t instanteDesactivacion=0x12D8;
constexpr uintptr_t bombaDesactivada=0x12DC;
constexpr uintptr_t tickHumo=0x1360; // Campo del esquema: C_SmokeGrenadeProjectile::m_nSmokeEffectTickBegin
constexpr uintptr_t humoActivo=0x13AA; // Campo del esquema: C_SmokeGrenadeProjectile::m_bSmokeEffectSpawned
constexpr uintptr_t origenHumo=0x1378; // Campo del esquema: C_SmokeGrenadeProjectile::m_vSmokeDetonationPos
constexpr uintptr_t explosionIniciada=0x12FC; // Campo del esquema: C_BaseCSGrenadeProjectile::m_bExplodeEffectBegan
constexpr uintptr_t fuegoTerminado=0x1A54; // Campo del esquema: C_Inferno::m_bInPostEffectTime
constexpr uintptr_t cantidadFuego=0x1A48; // Campo del esquema: C_Inferno::m_fireCount
constexpr uintptr_t tickFuego=0x1A5C; // Campo del esquema: C_Inferno::m_nFireEffectTickBegin
constexpr uintptr_t duracionFuego=0x1A50; // Campo del esquema: C_Inferno::m_nFireLifetime
constexpr uintptr_t tiempoJuego=0x30; // No incluido en el volcado; conservar valor previo.

}

