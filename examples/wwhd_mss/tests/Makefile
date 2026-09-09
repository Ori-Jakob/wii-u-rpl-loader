CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2

ifeq ($(OS),Windows_NT)
EXEEXT := .exe
endif

TARGET := macro_engine_test$(EXEEXT)
INPUT_TARGET := input_test$(EXEEXT)
SETTINGS_TARGET := settings_test$(EXEEXT)

# Only wut's headers are needed, for the VPAD and WPAD button bits.
WUT ?= $(if $(wildcard C:/devkitPro/wut/include),C:/devkitPro/wut,$(DEVKITPRO)/wut)

.PHONY: all test clean

all: $(TARGET) $(INPUT_TARGET) $(SETTINGS_TARGET)

$(TARGET): macro_engine_test.cpp ../macro_engine.cpp ../macro_engine.h
	$(CXX) $(CXXFLAGS) -I.. macro_engine_test.cpp ../macro_engine.cpp -o $@

$(INPUT_TARGET): input_test.cpp ../input.cpp ../input.h wut_host.h
	$(CXX) $(CXXFLAGS) -include wut_host.h -I.. -I../../../include -I$(WUT)/include \
		input_test.cpp ../input.cpp -o $@

$(SETTINGS_TARGET): settings_test.cpp ../settings.cpp ../settings.h ../macro_engine.h
	$(CXX) $(CXXFLAGS) -I.. -I../../../include settings_test.cpp ../settings.cpp -o $@

test: $(TARGET) $(INPUT_TARGET) $(SETTINGS_TARGET)
	./$(TARGET)
	./$(INPUT_TARGET)
	./$(SETTINGS_TARGET)

clean:
	$(RM) $(TARGET) $(INPUT_TARGET) $(SETTINGS_TARGET)
