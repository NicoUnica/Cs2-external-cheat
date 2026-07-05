# Nico CS2 External

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![ImGui](https://img.shields.io/badge/ImGui-1.90-brightgreen.svg)](https://github.com/ocornut/imgui)
[![DirectX](https://img.shields.io/badge/DirectX-11-purple.svg)](https://docs.microsoft.com/en-us/windows/win32/direct3d11)
[![Status](https://img.shields.io/badge/Status-Active-success.svg)]()

# CS2 External Overlay

External overlay para Counter-Strike 2 con interfaz gráfica moderna basada en ImGui.

## Descripción

Este proyecto es un overlay externo para CS2 que proporciona funcionalidades de ESP y utilidades de juego mediante una interfaz gráfica moderna y personalizable. Utiliza DirectX 11 para el renderizado y ImGui para la interfaz de usuario.

## Características

### ESP 
- **Skeleton ESP**: Visualización del esqueleto 3D de los jugadores con líneas conectadas
- **Name ESP**: Muestra el nombre de los jugadores
- **Weapon ESP**: Indica el arma que porta cada jugador
- **Distance ESP**: Muestra la distancia en metros
- **Health ESP**: Visualización de la salud con colores dinámicos (verde/amarillo/rojo)
- **Money ESP**: Muestra el dinero disponible del jugador
- **Flash ESP**: Indica si un jugador está cegado y el tiempo restante
- **Defusing ESP**: Alerta cuando un jugador está desactivando la C4
- **Team Check**: Opción para filtrar/excluir teammates

### Utilidades de Juego
- **Bomb Timer**: Temporizador de C4 que muestra el sitio (A/B) donde está plantada
- **Watermark**: Marca de agua con mi nombre
- **Stream Proof**: Modo invisible para OBS y capturas de pantalla

### Sistema de Configuración
- **Save/Load Config**: Guarda y carga configuraciones en archivo binario
- **Reset Config**: Restablece a valores predeterminados

### Interfaz de Usuario
- **Menú ImGui**: Interfaz moderna y personalizable
- **Colores Personalizables**: Sistema de colores RGBA para elementos ESP
- **Toggle Switches**: Interruptores personalizados en el menú
- **Responsive Design**: Interfaz que se adapta a diferentes resoluciones


## Dependencias

- **ImGui**: Biblioteca de interfaz de usuario inmediata
- **DirectX 11**: API de gráficos de Microsoft
- **Windows SDK**: Para funciones del sistema operativo

## Características Técnicas

- **External Overlay**: No inyecta código en el proceso del juego
- **Memory Reading**: Lectura de memoria del juego para obtener datos
- **World-to-Screen**: Transformación de coordenadas 3D a 2D
- **Bone ESP**: Sistema de esqueleto con 18 huesos por jugador
- **Dynamic Scaling**: Elementos ESP escalan según distancia
- **Thread-Safe**: Operaciones de memoria con mutex para seguridad
- **Anti-Detection**: Ofuscación de código y técnicas de protección

## Notas

- Este proyecto es solo con fines educativos por eso no esta subido


<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/a1ba52ec-5f62-4a3d-bac6-174664e85655" />



