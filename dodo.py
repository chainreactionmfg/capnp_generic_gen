import functools
import io
# Python3, if using Python2 install pathlib from pypi.
import pathlib

import cogapp


def task_json_cog():
    cog = cogapp.Cog()
    cog.options.bEofCanBeEnd = True
    globals = {'smaller': True}

    @listify
    def make_processors(filenames):
        def cog_process(filename):
            with open(filename, 'r') as input:
                oldVal = input.read()
            fIn = io.StringIO(oldVal)
            fOut = io.StringIO()
            cog.processFile(fIn, fOut, fname=filename, globals=globals)
            val = fOut.getvalue()
            if val != oldVal:  # Save some headache with unchanged outputs
                with open(filename, 'w') as output:
                    output.write(val)
        for filename in filenames:
            yield lambda: cog_process(filename)

    return {
        'actions': make_processors(['json.c++', 'generic.h']),
        'file_dep': ['json.c++', 'generic.h'],
    }


def task_json():
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

    compile_json = ('clang++ json.c++ {} {} {} {} -o %(targets)s'
                    ).format(clang_flags, rapidjson_flags,
                             deathhandler_flags, capnp_flags)
    file_deps.append('json.c++')
    file_deps.append('generic.h')
    return {'actions': [compile_json], 'targets': ['json'],
            'file_dep': file_deps, 'task_dep': ['json_cog']}


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
