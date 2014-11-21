import collections
import functools
import io
import os
# Python3, if using Python2 install pathlib from pypi.
import pathlib

import cogapp

visit_methods = collections.OrderedDict([
    ('file', ['Schema', 'schema::CodeGeneratorRequest::RequestedFile::Reader']),
    ('imports', ['Schema', 'List<Import>::Reader']),
    ('import', ['Schema', 'Import::Reader']),
    ('nested_decls', ['Schema']),
    ('decl', ['Schema', 'schema::Node::NestedNode::Reader']),
    ('struct_decl', ['Schema', 'schema::Node::NestedNode::Reader']),
    ('enum_decl', ['Schema', 'schema::Node::NestedNode::Reader']),
    ('const_decl', ['Schema', 'schema::Node::NestedNode::Reader']),
    ('annotation_decl', ['Schema', 'schema::Node::NestedNode::Reader']),
    ('annotation', ['schema::Annotation::Reader', 'Schema']),
    ('annotations', ['Schema']),
    ('type', ['Schema', 'schema::Type::Reader']),
    ('value', ['Schema', 'schema::Type::Reader', 'schema::Value::Reader']),
    ('struct_fields', ['StructSchema']),
    ('struct_default_value', ['StructSchema', 'capnp::StructSchema::Field']),
    ('struct_field', ['StructSchema', 'StructSchema::Field']),
    ('struct_field_slot', ['StructSchema', 'StructSchema::Field',
                           'schema::Field::Slot::Reader']),
    ('struct_field_group', ['StructSchema', 'StructSchema::Field',
                            'schema::Field::Group::Reader', 'Schema']),
    ('interface_decl', ['Schema', 'schema::Node::NestedNode::Reader']),
    ('param_list', ['InterfaceSchema', 'kj::String&', 'StructSchema']),
    ('method', ['InterfaceSchema', 'InterfaceSchema::Method']),
    ('methods', ['InterfaceSchema']),
    ('method_implicit_params', [
        'InterfaceSchema', 'InterfaceSchema::Method',
        'capnp::List<capnp::schema::Node::Parameter>::Reader']),
    ('enumerant', ['Schema', 'EnumSchema::Enumerant']),
    ('enumerants', ['Schema', 'EnumSchema::EnumerantList']),
])

python_keywords = [
    'and', 'as', 'assert', 'break', 'class', 'continue', 'def', 'del', 'elif',
    'else', 'except', 'exec', 'finally', 'for', 'from', 'global', 'if',
    'import', 'in', 'is', 'lambda', 'not', 'or', 'pass', 'print', 'raise',
    'return', 'try', 'while', 'with', 'yield'
]


class Cog(object):
    cog = cogapp.Cog()
    cog.options.bEofCanBeEnd = True
    globals = {
        'smaller_annotations': True,
        'visit_methods': visit_methods,
        'python_keywords': python_keywords,
    }
    filenames = ['json.c++', 'generic.h']

    @classmethod
    def create_doit_tasks(cls):
        for filename in cls.filenames:
            yield {
                'actions': [(cls.cog_process, (filename,))],
                'file_dep': [filename],
                'basename': 'cog_%s' % filename,
            }

    @classmethod
    def cog_process(cls, filename):
        with open(filename, 'r') as input:
            oldVal = input.read()
        fIn = io.StringIO(oldVal)
        fOut = io.StringIO()
        cls.cog.processFile(fIn, fOut, fname=filename, globals=cls.globals)
        val = fOut.getvalue()
        if val != oldVal:  # Save some headache with unchanged outputs
            with open(filename, 'w') as output:
                output.write(val)


def compile(cc_file):
    @listify
    def recurse_dir(dir):
        return (str(p) for p in pathlib.Path(dir).glob('**') if p.is_file())

    clang_flags = '-std=c++11 -fpermissive -Wall'
    rapidjson_flags = '-Irapidjson/include'
    file_deps = recurse_dir('rapidjson/include')
    capnp_flags = ' '.join([
        '-I../github_capnproto/c++/build/include',
        # Statically link capnp since that's easier than figuring out -L
        '../github_capnproto/c++/build/lib/libcapnp.a',
        '../github_capnproto/c++/build/lib/libkj.a'])
    file_deps.extend(recurse_dir('../github_capnproto/c++/build/include') +
                     ['../github_capnproto/c++/build/lib/libcapnp.a',
                      '../github_capnproto/c++/build/lib/libkj.a'])
    deathhandler_flags = ('-g -rdynamic -IDeathHandler '
                          'DeathHandler/death_handler.cc -ldl')
    file_deps.extend(recurse_dir('DeathHandler'))

    compile_json = ('clang++ {} {} {} {} {} -o %(targets)s'
                    ).format(cc_file, clang_flags, rapidjson_flags,
                             deathhandler_flags, capnp_flags)
    file_deps.append(cc_file)
    file_deps.append('generic.h')
    base_name = os.path.splitext(cc_file)[0]
    return {'actions': [compile_json],
            'targets': [base_name], 'file_dep': file_deps,
            'task_dep': ['cog_%s' % cc_file, 'cog_generic.h'],
            'basename': 'compile_%s' % cc_file}


def task_compile_cc():
    for filename in ['json.c++']:
        yield compile(filename)


# From:
# https://github.com/shazow/unstdlib.py/blob/e2cf942330630381aee6a843fd1d379fe98d1edb/unstdlib/standard/list_.py#L149
def listify(fn=None, wrapper=list):
    """
    A decorator which wraps a function's return value in ``list(...)``.

    Useful when an algorithm can be expressed more cleanly as a generator but
    the function should return an list.

    Example::

        >>> @listify
        ... def get_lengths(iterable):
        ...     for i in iterable:
        ...         yield len(i)
        >>> get_lengths(["spam", "eggs"])
        [4, 4]
        >>>
        >>> @listify(wrapper=tuple)
        ... def get_lengths_tuple(iterable):
        ...     for i in iterable:
        ...         yield len(i)
        >>> get_lengths_tuple(["foo", "bar"])
        (3, 3)
    """
    def listify_return(fn):
        @functools.wraps(fn)
        def listify_helper(*args, **kw):
            return wrapper(fn(*args, **kw))
        return listify_helper
    if fn is None:
        return listify_return
    return listify_return(fn)
