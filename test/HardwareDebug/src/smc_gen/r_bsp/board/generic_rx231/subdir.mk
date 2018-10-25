################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/smc_gen/r_bsp/board/generic_rx231/dbsct.c \
../src/smc_gen/r_bsp/board/generic_rx231/hwsetup.c \
../src/smc_gen/r_bsp/board/generic_rx231/lowlvl.c \
../src/smc_gen/r_bsp/board/generic_rx231/lowsrc.c \
../src/smc_gen/r_bsp/board/generic_rx231/resetprg.c \
../src/smc_gen/r_bsp/board/generic_rx231/sbrk.c \
../src/smc_gen/r_bsp/board/generic_rx231/vecttbl.c 

COMPILER_OBJS += \
src/smc_gen/r_bsp/board/generic_rx231/dbsct.obj \
src/smc_gen/r_bsp/board/generic_rx231/hwsetup.obj \
src/smc_gen/r_bsp/board/generic_rx231/lowlvl.obj \
src/smc_gen/r_bsp/board/generic_rx231/lowsrc.obj \
src/smc_gen/r_bsp/board/generic_rx231/resetprg.obj \
src/smc_gen/r_bsp/board/generic_rx231/sbrk.obj \
src/smc_gen/r_bsp/board/generic_rx231/vecttbl.obj 

C_DEPS += \
src/smc_gen/r_bsp/board/generic_rx231/dbsct.d \
src/smc_gen/r_bsp/board/generic_rx231/hwsetup.d \
src/smc_gen/r_bsp/board/generic_rx231/lowlvl.d \
src/smc_gen/r_bsp/board/generic_rx231/lowsrc.d \
src/smc_gen/r_bsp/board/generic_rx231/resetprg.d \
src/smc_gen/r_bsp/board/generic_rx231/sbrk.d \
src/smc_gen/r_bsp/board/generic_rx231/vecttbl.d 

# Each subdirectory must supply rules for building sources it contributes
src/smc_gen/r_bsp/board/generic_rx231/%.obj: ../src/smc_gen/r_bsp/board/generic_rx231/%.c src/smc_gen/r_bsp/board/generic_rx231/Compiler.sub
	@echo 'Scanning and building file: $<'
	@echo 'Invoking: Scanner and Compiler'
	@echo src\smc_gen\r_bsp\board\generic_rx231\cDepSubCommand.tmp=
	@sed -e "s/^/    /" "src\smc_gen\r_bsp\board\generic_rx231\cDepSubCommand.tmp"
	ccrx -subcommand="src\smc_gen\r_bsp\board\generic_rx231\cDepSubCommand.tmp" -output=dep="$(@:%.obj=%.d)"  -MT="$(@:%.d=%.obj)"  -MT="$(@:%.obj=%.d)" "$<"
	@echo src\smc_gen\r_bsp\board\generic_rx231\cSubCommand.tmp=
	@sed -e "s/^/    /" "src\smc_gen\r_bsp\board\generic_rx231\cSubCommand.tmp"
	ccrx -subcommand="src\smc_gen\r_bsp\board\generic_rx231\cSubCommand.tmp" "$<"
	@echo 'Finished Scanning and building: $<'
	@echo.

