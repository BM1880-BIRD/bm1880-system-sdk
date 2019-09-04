import sys

if len(sys.argv) < 3:
	print("usage: sort.py fixed_files.txt full_files.txt")
	exit(-1)

print("# fixed: " + sys.argv[1])
print("# full: " + sys.argv[2])

fixed_file = open(sys.argv[1], 'r')
full_file = open(sys.argv[2], 'r')

fixed_lines = fixed_file.readlines()
full_lines = full_file.readlines()

print("# fixed lines number: " + str(len(fixed_lines)))
print("# full lines number: " + str(len(full_lines)))

final_lines = []
fixed_files = []

for line in fixed_lines:
	if line[0] == '#' or len(line.split(' ')) < 4:
		continue
	fixed_files.append(line.split(' ')[1])
	final_lines.append(line)

for line in full_lines:
	if line[0] == '#' or len(line.split(' ')) < 4:
		continue
	if line.split(' ')[1] not in fixed_files:
		final_lines.append(line)

final_lines.sort();
for line in final_lines:
	print(line.strip())
