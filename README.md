# Nico CS2 External

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![Windows x64](https://img.shields.io/badge/Windows-x64-purple.svg)](https://www.microsoft.com/windows)
[![Versión parcial](https://img.shields.io/badge/Estado-Parcial-orange.svg)](#funciones-no-incluidas)

Núcleo externo para CS2 escrito en C++20 para consultar datos del juego y actualizarlos en segundo plano.

## Características

- Conexión con el proceso y lectura de memoria con validaciones.
- Datos de jugadores, armas, salud, armadura, dinero y estados.
- Información de espectadores, objetos y granadas.
- Estado de la bomba, temporizadores y contador de impactos.
- Vectores, matrices, proyección y posiciones de huesos.
- Hilo de lectura, cachés, sincronización y control de frecuencia.

## Uso

Esta versión contiene la base técnica para integrarla en una aplicación propia. No incluye un ejecutable listo para usar.

Los archivos de interfaz, inicio y ajustes están vacíos intencionadamente para que cada persona pueda añadir su implementación. La carpeta de recursos también se deja vacía.

## Funciones no incluidas

La UI, el menú, el overlay y sus recursos gráficos no forman parte de esta distribución. Tampoco se incluyen **aimbot, seed triggerbot, external chams, flash chams** y muchas otras funciones, ni la persistencia de preferencias o el audio.

De momento no voy a publicar el resto de funciones. Son características que suelen ofrecerse en cheats premium y prefiero mantener esa parte privada.
