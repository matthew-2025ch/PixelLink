#include"Bus_test.hpp"
#include"CPU_test.hpp"
#include"Interrupt_test.hpp"
#include"Cartridge_test.hpp"
auto main() -> int {
	BusTest::BusTest();
	CPUTest::CPUTest();
	InterruptTest::InterruptTest();
	CartridgeTest::CartridgeTest();
	return 0;
}