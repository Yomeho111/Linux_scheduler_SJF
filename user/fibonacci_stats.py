import os

def helper_function(lines):
    fibonacci_map = {}
    max_value = 0
    avg_column2 = 0
    avg_column3 = 0
    for line in lines:
        temp = int(line.split(',')[-1])
        fibonacci_map[temp] = [line.split(',')[-2] , line.split(',')[-1].strip('\n')]
        max_value = max(max_value, temp)
        avg_column2 += int(line.split(',')[2])
        avg_column3 += int(line.split(',')[3])
    print("process with the largest total time: runqueue time",  fibonacci_map[max_value][0], ", total time: ",fibonacci_map[max_value][1])
    print("avg_rqtime: ", avg_column2/len(lines), ", avg_totaltime: ", avg_column3/len(lines))


current_directory = os.getcwd()
all_files = os.listdir(current_directory)
text_files = [file for file in all_files if file.endswith(".txt") and 'taskset' in file]
for text_file in text_files:
    file = open(text_file, "r")
    lines = file.readlines()
    print('********' + text_file + '********')
    helper_function(lines)






