import matplotlib.pyplot as plt
import csv
from sys import argv

if len(argv) < 3:
    print("please input cmd: {python3 csv_display.py file1.csv file2.csv y_limit_top}")
    print(argv)
    exit(0)

fd_1 = open(argv[1], 'r')
csv_file_1 = csv.reader(fd_1)
fd_2 = open(argv[2], 'r')
csv_file_2 = csv.reader(fd_2)

title_1 = next(csv_file_1)
ylabel_1 = next(csv_file_1)
line_label_1 = next(csv_file_1)
print(title_1, ylabel_1, line_label_1)

title_2 = next(csv_file_2)
ylabel_2 = next(csv_file_2)
line_label_2 = next(csv_file_2)
print(title_2, ylabel_2, line_label_2)

# buf_w,buf_r = [],[]
buf_1 = []
buf_2 = []

for row in csv_file_1:
    buf_1.append(float(row[0]))
print("size_1=", len(buf_1))

for row in csv_file_2:
    buf_2.append(float(row[0]))
print("size_2=", len(buf_2))


plt.title(title_1[1]+" vs "+title_2[1])
plt.ylabel(ylabel_1[1])

plt.plot(buf_1, label=line_label_1[1])
plt.plot(buf_2, label=line_label_2[1], alpha=0.8)

if len(argv) == 4:
    ylim_top = float(argv[3])
    plt.ylim(top=ylim_top)

plt.legend()
plt.show()
