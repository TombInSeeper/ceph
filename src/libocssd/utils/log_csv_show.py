import matplotlib.pyplot as plt
import csv
from sys import argv

fd = open(argv[1], 'r')
csv_file = csv.reader(fd)

title = next(csv_file)
ylabel = next(csv_file)
line_label = next(csv_file)
print(title)
print(ylabel)
print(line_label)

buf = []
for row in csv_file:
    buf.append(float(row[0]))
print("size=", len(buf))

plt.title(title[1])
plt.ylabel(ylabel[1])

plt.plot(buf, label=line_label[1])
plt.legend()
plt.show()
