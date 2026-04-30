import sys

def main():
    if len(sys.argv) < 2:
        print("usage: python file2header.py <filename> | (optional) <outputname>")
        sys.exit(-1)
    
    with open(sys.argv[1], 'rb') as infile:

        ofname = sys.argv[1].split('.')[0] + ".h" if len(sys.argv) < 3 else sys.argv[2]

        with open(ofname, "r") as outfile:

            inarray = infile.read()

            outfile.write("#pragma once\n")
            outfile.write(f"const uint8_t {ofname.split('.')[0]}[] = {'{'}")
            