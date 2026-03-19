import sys

def main():

    for fname in sys.argv[1:]:
        with open(fname, "r") as inputfile:
            