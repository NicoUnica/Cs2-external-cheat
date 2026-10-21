#pragma once
#include <cmath>
#include <cstdint>
#include <array>
#include <utility>

// Vector 2D para coordenadas de pantalla
struct Vector2 {
  float x, y;
  Vector2() : x(0), y(0) {}
  Vector2(float x, float y) : x(x), y(y) {}
};

// Vector 3D para coordenadas del mundo
struct Vector3 {
  float x, y, z;
  Vector3() : x(0), y(0), z(0) {}
  Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
  bool EsCero() const { return x == 0 && y == 0 && z == 0; }

  Vector3 operator-(const Vector3 &v) const {
    return Vector3(x - v.x, y - v.y, z - v.z);
  }
  Vector3 operator+(const Vector3 &v) const {
    return Vector3(x + v.x, y + v.y, z + v.z);
  }
  Vector3 operator*(float f) const { return Vector3(x * f, y * f, z * f); }

};

// Matriz de vista para proyección 3D a 2D
struct MatrizVista {
  float m[4][4];
};

// Datos de un jugador enemigo o aliado
struct DatosJugador {
  uintptr_t personaje;
  uintptr_t controlador;
  int salud;
  uint8_t equipo;
  bool estaVivo;
  bool esValido;
  Vector3 origen;
  char nombre[128];
  char nombreArma[128];
  float distancia;
  int dinero;
  bool cegado=false;
  int armadura=-1;
  std::uint64_t sonidoEn=0;
  bool casco=false,kit=false,desactivando=false,inmune=false,recargando=false;
  std::uint16_t armaId=0;
  int balasCargador=-1,balasReserva=-1;
  bool estaApuntando=false,apuntadoLeido=false,llevaBomba=false;
};

// Los huesos pertenecen al cuadro de dibujo, no a los datos compartidos entre hilos.
struct PoseJugador {
  uintptr_t escena=0;
  Vector3 posicionesHuesos[24]{};
};
inline bool MuestraReciente(std::uint64_t leidoEn,std::uint64_t ahora,std::uint64_t limite=300) {
  return leidoEn && ahora>=leidoEn && ahora-leidoEn<=limite;
}

// Datos del jugador local
struct JugadorLocal {
  uintptr_t personaje;
  int salud;
  uint8_t equipo;
  bool esValido;
  Vector3 origen;
  bool estaApuntando=false,apuntadoLeido=false;
  std::uint16_t identificadorArma=0;
  // La vista puede seguir activa cuando el personaje local está muerto.
  uintptr_t personajeVista=0;
  Vector3 origenVista{};
  uint8_t equipoVista=0;
  bool vistaValida=false;
  bool OcultarJugador(uintptr_t personajeJugador,uint8_t equipoJugador,bool filtrar) const {
    return personajeJugador==personaje || (personajeVista&&personajeJugador==personajeVista) ||
        (filtrar&&(equipoVista==2||equipoVista==3)&&equipoJugador==equipoVista);
  }
};

enum IndiceHueso : int {
    PELVIS = 1,
    COLUMNA0 = 2,
    COLUMNA1 = 3,
    COLUMNA2 = 4,
    CUELLO = 6,
    CABEZA = 7,
    CLAVICULA_IZQUIERDA = 8,
    HOMBRO_IZQUIERDO = 9,
    CODO_IZQUIERDO = 10,
    MANO_IZQUIERDA = 11,
    CLAVICULA_DERECHA = 12,
    HOMBRO_DERECHO = 13,
    CODO_DERECHO = 14,
    MANO_DERECHA = 15,
    CADERA_IZQUIERDA = 17,
    RODILLA_IZQUIERDA = 18,
    TALON_IZQUIERDO = 19,
    CADERA_DERECHA = 20,
    RODILLA_DERECHA = 21,
    TALON_DERECHO = 22,
    PUNTA_IZQUIERDA = 24,
    PUNTA_DERECHA = 25
};

// Convertir coordenadas 3D del mundo a 2D de pantalla
inline bool ProyectarPantalla(const Vector3 &posicionMundo, Vector2 &posicionPantalla,
                          const MatrizVista &matriz, int anchoPantalla, int altoPantalla) {
  float anchura = matriz.m[3][0] * posicionMundo.x + matriz.m[3][1] * posicionMundo.y +
            matriz.m[3][2] * posicionMundo.z + matriz.m[3][3];
  if (anchoPantalla <= 0 || altoPantalla <= 0 || !std::isfinite(anchura) || anchura < 0.001f)
    return false;

  float x = matriz.m[0][0] * posicionMundo.x + matriz.m[0][1] * posicionMundo.y +
            matriz.m[0][2] * posicionMundo.z + matriz.m[0][3];
  float y = matriz.m[1][0] * posicionMundo.x + matriz.m[1][1] * posicionMundo.y +
            matriz.m[1][2] * posicionMundo.z + matriz.m[1][3];

  posicionPantalla.x = (anchoPantalla / 2.f) + (x / anchura) * (anchoPantalla / 2.f);
  posicionPantalla.y = (altoPantalla / 2.f) - (y / anchura) * (altoPantalla / 2.f);
  return std::isfinite(posicionPantalla.x) && std::isfinite(posicionPantalla.y);
}

// Calcular distancia euclidiana entre dos puntos 3D
inline float Distancia3D(const Vector3 &a, const Vector3 &b) {
  const Vector3 diferencia = a - b;
  return sqrtf(diferencia.x * diferencia.x + diferencia.y * diferencia.y + diferencia.z * diferencia.z);
}

namespace esqueleto {
// Conexiones de Vesta 1.2.5: articulaciones reales, sin puntos interpolados.
inline constexpr int PechoMotor=23;
inline constexpr int CantidadHuesos=PechoMotor+1;
inline constexpr std::array<std::pair<int,int>,18> Conexiones{{
    {PELVIS,COLUMNA0},{COLUMNA0,COLUMNA1},{COLUMNA1,COLUMNA2},
    {COLUMNA2,PechoMotor},{PechoMotor,CUELLO},{CUELLO,CABEZA},
    {CUELLO,HOMBRO_IZQUIERDO},{HOMBRO_IZQUIERDO,CODO_IZQUIERDO},{CODO_IZQUIERDO,MANO_IZQUIERDA},
    {CUELLO,HOMBRO_DERECHO},{HOMBRO_DERECHO,CODO_DERECHO},{CODO_DERECHO,MANO_DERECHA},
    {PELVIS,CADERA_IZQUIERDA},{CADERA_IZQUIERDA,RODILLA_IZQUIERDA},{RODILLA_IZQUIERDA,TALON_IZQUIERDO},
    {PELVIS,CADERA_DERECHA},{CADERA_DERECHA,RODILLA_DERECHA},{RODILLA_DERECHA,TALON_DERECHO}
}};
inline constexpr auto HuesosUsados=[] {
    std::array<bool,CantidadHuesos> mascara{};
    for(const auto& [a,b]:Conexiones) { mascara[a]=true; mascara[b]=true; }
    return mascara;
}();
static_assert(CantidadHuesos==sizeof(PoseJugador{}.posicionesHuesos)/sizeof(Vector3));
}

namespace optimizacion {
inline float DistanciaCuadrada(Vector3 a,Vector3 b) {
    const auto d=a-b; return d.x*d.x+d.y*d.y+d.z*d.z;
}
struct Frustum {
    struct Plano { std::array<float,4> coeficientes; float radio; };
    std::array<Plano,6> planos{};
    explicit Frustum(const MatrizVista& matriz) {
        int indice=0;
        for(int eje=0;eje<3;++eje) for(float signo:{-1.f,1.f}) {
            auto& p=planos[indice++];
            for(int k=0;k<4;++k) p.coeficientes[k]=matriz.m[3][k]+signo*matriz.m[eje][k];
            const auto& c=p.coeficientes; p.radio=512.f*std::sqrt(c[0]*c[0]+c[1]*c[1]+c[2]*c[2]);
        }
    }
    bool Fuera(Vector3 posicion) const {
        for(const auto& p:planos) {
            const auto& c=p.coeficientes;
            const float d=c[0]*posicion.x+c[1]*posicion.y+c[2]*posicion.z+c[3];
            if(std::isfinite(d)&&std::isfinite(p.radio)&&d < -p.radio) return true;
        }
        return false;
    }
};
}

namespace armas {
struct Dato { std::uint16_t id; const char* nombre; bool recarga; };
inline constexpr auto Catalogo=std::to_array<Dato>({
    {1,"Deagle",true},{2,"Dual Berettas",true},{3,"Five-SeveN",true},{4,"Glock",true},
    {7,"AK-47",true},{8,"AUG",true},{9,"AWP",true},{10,"FAMAS",true},
    {11,"G3SG1",true},{13,"Galil",true},{14,"M249",true},{16,"M4A4",true},
    {17,"MAC-10",true},{19,"P90",true},{23,"MP5-SD",true},{24,"UMP-45",true},
    {25,"XM1014",true},{26,"PP-Bizon",true},{27,"MAG-7",true},{28,"Negev",true},
    {29,"Sawed-off",true},{30,"Tec-9",true},{31,"Zeus",false},{32,"P2000",true},
    {33,"MP7",true},{34,"MP9",true},{35,"Nova",true},{36,"P250",true},
    {38,"SCAR-20",true},{39,"SG 553",true},{40,"SSG 08",true},{42,"Cuchillo",false},
    {43,"Flash",false},{44,"HE",false},{45,"Humo",false},{46,"Molotov",false},
    {47,"Señuelo",false},{48,"Incendiaria",false},{49,"C4",false},{59,"Cuchillo",false},
    {60,"M4A1-S",true},{61,"USP-S",true},{63,"CZ75",true},{64,"R8",true}
});
constexpr const Dato* Buscar(std::uint16_t id) {
    for(const auto& dato:Catalogo) if(dato.id==id) return &dato;
    return nullptr;
}
constexpr bool PuedeRecargar(std::uint16_t id) { const auto* dato=Buscar(id); return dato&&dato->recarga; }
}
