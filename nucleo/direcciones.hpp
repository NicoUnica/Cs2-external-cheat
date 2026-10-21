#pragma once
#include <cstdint>
// Volcado local de cs2-dumper: compilación 14191, 2026-10-21.
namespace direcciones::globales::cliente {
inline constexpr uintptr_t listaEntidades=0x271A908;
inline constexpr uintptr_t controladorLocal=0x253CB78;
inline constexpr uintptr_t personajeLocal=0x2565718;
inline constexpr uintptr_t matrizVista=0x256B020;
inline constexpr uintptr_t bombaPlantada=0x24CB3C0;
inline constexpr uintptr_t armaBomba=0x24C9B30;
inline constexpr uintptr_t variablesGlobales=0x2230090;
}
namespace direcciones::globales::motor {
// Procedencia: offsets.hpp del mismo volcado local, compilación 14191.
inline constexpr int compilacionEsperada=14191;
inline constexpr uintptr_t compilacion=0x61CFEC;
inline constexpr uintptr_t clienteRed=0x91AFC0;
inline constexpr uintptr_t tickCliente=0x398;
inline constexpr uintptr_t estadoConexion=0x230;
inline constexpr uintptr_t mapaFondo=0x2C143F;
}

// Campos contrastados con client_dll.json, 2026-10-21T15:18:39.825258800+00:00.
namespace campos {
constexpr uintptr_t identidad=0x10; // CEntityInstance
constexpr uintptr_t nombreInterno=0x20; // CEntityIdentity

constexpr uintptr_t nodoEscena=0x330;
constexpr uintptr_t salud=0x34C;
constexpr uintptr_t estadoVida=0x354;
constexpr uintptr_t equipo=0x3E7;

constexpr uintptr_t servicioArmas=0x12F0;
constexpr uintptr_t origenAnterior=0x14A4;

constexpr uintptr_t tiempoCeguera=0x14FC; // C_CSPlayerPawnBase::m_flFlashBangTime
constexpr uintptr_t servicioObjetos=0x12F8; // C_BasePlayerPawn::m_pItemServices
constexpr uintptr_t armadura=0x1EC8; // C_CSPlayerPawn::m_ArmorValue
constexpr uintptr_t instanteSonido=0x1EA0; // C_CSPlayerPawn::m_flEmitSoundTime
constexpr uintptr_t casco=0x49,kit=0x48; // CCSPlayer_ItemServices
constexpr uintptr_t desactivando=0x1E9A,inmune=0x3508; // C_CSPlayerPawn
constexpr uintptr_t recargando=0x1A3C; // C_CSWeaponBase::m_bInReload
constexpr uintptr_t balasCargador=0x1928; // C_BasePlayerWeapon::m_iClip1
constexpr uintptr_t balasReserva=0x1930; // C_BasePlayerWeapon::m_pReserveAmmo[0]

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

constexpr uintptr_t apuntando=0x1E98;
constexpr uintptr_t inventarioArmas=0x48;
constexpr uintptr_t atributos=0x1290;
constexpr uintptr_t objeto=0x50;
constexpr uintptr_t definicionObjeto=0x1BA;
constexpr uintptr_t servicioBalas=0x1570;
constexpr uintptr_t impactosServidor=0x48;
constexpr uintptr_t propietario=0x520;
constexpr uintptr_t bombaActiva=0x1288;
constexpr uintptr_t zonaBomba=0x128C;
constexpr uintptr_t instanteExplosion=0x12B8;
constexpr uintptr_t duracionTemporizador=0x12C0;
constexpr uintptr_t desactivacionActiva=0x12C4;
constexpr uintptr_t instanteDesactivacion=0x12D8;
constexpr uintptr_t bombaDesactivada=0x12DC;
constexpr uintptr_t tickHumo=0x1360; // C_SmokeGrenadeProjectile::m_nSmokeEffectTickBegin
constexpr uintptr_t humoActivo=0x13AA; // C_SmokeGrenadeProjectile::m_bSmokeEffectSpawned
constexpr uintptr_t origenHumo=0x1378; // C_SmokeGrenadeProjectile::m_vSmokeDetonationPos
constexpr uintptr_t explosionIniciada=0x12FC; // C_BaseCSGrenadeProjectile::m_bExplodeEffectBegan
constexpr uintptr_t fuegoTerminado=0x1A54; // C_Inferno::m_bInPostEffectTime
constexpr uintptr_t cantidadFuego=0x1A48; // C_Inferno::m_fireCount
constexpr uintptr_t tickFuego=0x1A5C; // C_Inferno::m_nFireEffectTickBegin
constexpr uintptr_t duracionFuego=0x1A50; // C_Inferno::m_nFireLifetime
constexpr uintptr_t tiempoJuego=0x30; // No incluido en el volcado; conservar valor previo.
// CGlobalVars: ruta del mapa, comprobada en vivo en la compilación 14190; pendiente de verificar en 14191.
constexpr uintptr_t nombreMapa=0x180;

}

