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

## Actualización del núcleo

- Offsets actualizados al volcado local de la compilación **14192** de CS2.
- Inicialización con resultados específicos para proceso, permisos, módulos, versión y ventana.
- Lecturas agrupadas y caché de páginas de entidades; datos compartidos accesibles mediante copias sincronizadas.
- Observación automática opcional y separación entre el personaje local y la cámara observada.
- Detección de daño con conciliación de contadores para evitar avisos repetidos.
- Objetos con su última posición leída, sin predicción de movimiento; se mantienen los tiempos de humo y fuego.
- Eliminados la lectura del nombre del mapa, el seguimiento de sonido ESP y el código de errores sin uso.

La versión del juego se valida al conectar. Los offsets dependen de esa compilación; `matrizHuesos` y `tiempoJuego` conservan sus valores anteriores porque no estaban presentes en el volcado. Esta actualización se ha compilado en Windows x64, pero no se ha validado dentro de una partida.

## Compilación

Requiere Windows x64, Visual Studio 2022 con las herramientas de C++, Windows SDK y CMake 3.20 o posterior.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Se genera la biblioteca estática `build/Release/nico.lib`. No necesita ImGui, DirectX ni recursos gráficos.

## Organización

- `nucleo/juego.hpp` y `juego.cpp`: proceso, lecturas, opciones y ciclo del hilo.
- `nucleo/datos.hpp` y `datos.cpp`: bomba, daño, objetos y granadas.
- `nucleo/tipos.hpp`: estructuras, vectores, proyección, huesos y catálogo de armas.
- `nucleo/direcciones.hpp`: offsets y versión esperada del juego.

## Uso

Esta versión contiene la base técnica para integrarla en una aplicación propia. No incluye un ejecutable listo para usar.

Los archivos de interfaz, inicio y ajustes están vacíos intencionadamente para que cada persona pueda añadir su implementación. La carpeta de recursos también se deja vacía.

La aplicación que integre la biblioteca selecciona los datos mediante `EstablecerLecturas(opciones, frecuencia, distanciaObjetos)`. La frecuencia se limita a 10–250 Hz y la distancia se expresa en metros. El bit `lecturas::ObservacionAutomatica` habilita el uso del equipo de la vista observada.

`IniciarMemoria()` devuelve ahora `ResultadoMemoria`; hay que comprobar `ResultadoMemoria::Disponible` antes de llamar a `IniciarHiloMemoria()`. Para consultar datos se usan `CopiarDatosJuego()`, `CopiarEspectadores()`, `CopiarDatosExtras()` y `mundo::CopiarDatos()`. Al finalizar, se llama a `DetenerHiloMemoria()` y después a `LiberarMemoria()`.

## Funciones no incluidas

La UI, el menú, el overlay y sus recursos gráficos no forman parte de esta distribución. Tampoco se incluyen **aimbot, seed triggerbot, external chams, flash chams** y muchas otras funciones, ni la persistencia de preferencias o el audio.

De momento no voy a publicar el resto de funciones. Son características que suelen ofrecerse en cheats premium y prefiero mantener esa parte privada.
