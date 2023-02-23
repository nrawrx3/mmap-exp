import random
import argparse

STRING_LENGTH = 3

def generate_random(count, dups):
    numbers = list(range(0, count))
    random.shuffle(numbers)
    return numbers

def generate_random_strings(count):
    strings = [None] * count
    letters = 'abcdefghijklmnopqrstuvwxyz'
    indices = list(range(0, len(letters)))
    for i in range(0, count):
        strlength = random.randint(1, STRING_LENGTH)
        indices_copy = indices[:]
        random.shuffle(indices_copy)
        strings[i] = ''.join(letters[indices_copy[i]] for i in range(0, STRING_LENGTH))
    return strings


def write_to_file(numbers, file, strings=None):
    with open(file, mode='w') as f:
        for i, n in enumerate(numbers):
            if strings is None:
                f.write(str(n) + '\n')
            else:
                f.write('{} {}\n'.format(n, strings[i]))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()

    ap.add_argument('count', type=int, help='Number of unique keys')
    ap.add_argument('-d', '--dups', type=int, default=0, help='Number of duplicates')
    ap.add_argument('-o', '--output', type=str, default='rbt_keys.txt')
    ap.add_argument('-s', '--strings', action='store_true', help='Put random string values')
    ap.add_argument('--sorted', action='store_true', help='Print keys in sorted order')

    args = ap.parse_args()

    count = args.count
    dups = args.dups

    numbers = generate_random(count, dups)
    strings = None

    if args.strings:        
        strings = generate_random_strings(count)

    if args.sorted:
        numbers.sort()

    write_to_file(numbers, args.output, strings)