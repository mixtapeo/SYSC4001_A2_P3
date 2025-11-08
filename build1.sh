if [ ! -d "bin" ]; then
    mkdir bin
else
	rm bin/*
fi
g++ -g -O0 -I . -o bin/interrupts interrupts_101304877_101294162.cpp
cp output_files/1/program1.txt .
cp output_files/1/program2.txt .
./bin/interrupts input_files/1/trace.txt input_files/1/vector_table.txt input_files/1/device_table.txt input_files/1/external_files.txt
rm program1.txt
rm program2.txt
cat execution.txt
echo "-------SYSTEM STATUS--------"
cat system_status.txt