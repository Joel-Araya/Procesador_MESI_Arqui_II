# ==============================================================================
# VARIABLES GLOBALES
# ==============================================================================
CXX = g++
CXXFLAGS = -std=c++17 -Wall -g -pthread

# Directorios de código fuente
SRCDIR = src
COMPONENTS = $(SRCDIR)/components
INTERCONNECT = $(SRCDIR)/interconnect
UTILS = $(SRCDIR)/utils

# Archivo ejecutable final
TARGET = sim_mesi


# ==============================================================================
# ARCHIVOS FUENTE Y OBJETOS
# ==============================================================================

# Lista explícita de todos los archivos fuente
SRCS = $(SRCDIR)/main.cpp \
       $(INTERCONNECT)/BusInterconnect.cpp \
       $(COMPONENTS)/cacheL1.cpp \
       $(COMPONENTS)/memory.cpp


# Mapea src/dir/file.cpp a obj/dir/file.o
OBJS = $(patsubst $(SRCDIR)/%.cpp, obj/%.o, $(SRCS))

# ==============================================================================
# REGLAS
# ==============================================================================

.PHONY: all clean run test

all: $(TARGET)

# 1. Regla para construir el ejecutable final (sim_mesi)
$(TARGET): obj $(OBJS)
	@echo "🔗 Enlazando el ejecutable..."
	$(CXX) $(OBJS) -o $@ $(CXXFLAGS)

# 2. Regla para crear el directorio de objetos (asegura que obj/components exista)
obj:
	@mkdir -p obj/components obj/interconnect obj/utils

# 3. Regla general para compilar archivos .cpp a .o (Pattern Rule)
# Compila cualquier archivo .cpp en el directorio fuente o subdirectorios
obj/%.o: $(SRCDIR)/%.cpp
	@echo "⚙️ Compilando $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ------------------------------------------------------------------------------
# REGLAS ADICIONALES
# ------------------------------------------------------------------------------

clean:
	@echo "🧹 Limpiando archivos temporales y ejecutables..."
	@rm -rf $(TARGET) obj/

run: all
	@echo "🚀 Ejecutando el Simulador MESI..."
	./$(TARGET)

test: all
	@echo "🧪 Ejecutando prueba de concurrencia básica..."
	./$(TARGET) test_mode

# ------------------------------------------------------------------------------
# Manejo de dependencias
# ------------------------------------------------------------------------------

-include $(OBJS:.o=.d)