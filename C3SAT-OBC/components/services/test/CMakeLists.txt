## Unity component test for the telecommand codec.
## Run with:  idf.py -T services build flash monitor
## (or via the unit-test-app / QEMU). Only the pure codec functions are tested
## here, so no FreeRTOS objects or hardware are required.
idf_component_register(
    SRCS "test_telecommand.c"
    REQUIRES unity services
    WHOLE_ARCHIVE
)
