from gdb import printing as gdb_printing
import re

MAX_ENTRIES_SHOWN = 16

def array_info_string(size, capacity, start, end):
    r = 'fo::Array with size = {}, cap = {}, start = {}, end = {}'.format(size, capacity, start, end)
    return r

class ScaffoldArrayIterator:
    def __init__(self, value):
        self.index = 0
        self.current = value['_data']
        self.end = value['_data'] + value['_size']
        self.size = value['_size']
        self.cap = value['_capacity']
        
    def __iter__(self):
        return self
        
    def __next__(self):
        if self.index == self.end:
            raise StopIteration()
            
        ret = ("[{}]".format(self.index), self.current.dereference())
        self.index += 1
        return ret
        

class ScaffoldArrayPrinter:
    def __init__(self, value):
        self.value = value
       
    def to_string(self):
        data = self.value['_data']
        end = data + self.value['_size']
        return array_info_string(self.value['_size'], self.value['_capacity'], data, end)
        
    def children(self):
        return ScaffoldArrayIterator(self.value)


class ScaffoldPodHashPrinter:
    def __init__(self, value):
        self.value = value

    def to_string(self):
        hashes = self.value['_hashes']
        entries = self.value['_entries']
        max_load_factor = self.value['_load_factor']

        num_entries = entries['_size']
        num_hash_slots_allocated = hashes['_size']

        s = 'fo::PodHash with num_entries = {}, num_hash_slots_allocated = {}.'
        return s.format(num_entries, num_hash_slots_allocated)

    def children(self):
        return ScaffoldPodHashIterator(self.value['_entries'])


class ScaffoldPodHashIterator:
    def __init__(self, entries):
        self.entries = entries
        self.num_iterated = 0
        self.count = entries['_size']

    def __iter__(self):
        return self

    def __next__(self):
        if self.num_iterated == self.count or self.num_iterated > MAX_ENTRIES_SHOWN:
            raise StopIteration()

        if self.num_iterated == MAX_ENTRIES_SHOWN:
            remaining_items = self.count - self.num_iterated
            self.num_iterated += 1

            return ('[... {} more entries]'.format(remaining_items), '')

        data = self.entries['_data']

        ret = ('[{}]'.format(data[self.num_iterated]['key']), data[self.num_iterated]['value'])
        self.num_iterated += 1
        return ret


array_regex = re.compile(r'^fo::Array<.*>$')
podhash_regex = re.compile(r'^fo::PodHash<.*>$')

def make_pretty_printer():
    pp = gdb_printing.RegexpCollectionPrettyPrinter('scaffold')

    pp.add_printer('fo_Array', array_regex, ScaffoldArrayPrinter)
    pp.add_printer('fo_PodHash', podhash_regex, ScaffoldPodHashPrinter)

    return pp
