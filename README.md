Capnproto Generic Generator
===========================

This is a generic framework for generating output from a Cap'n proto schema file.

Based a little on how Clang's ASTVisitor works, except it uses virtual methods
because I was too lazy to do the 'curiously repeating template' trick to save a
vtable, since schemas tend not to be so big or so often parsed that performance
of vtables in the visitor class would make much difference to anybody.

Basically, given a schema file, the header will load it into a schemaLoader
instance. Every traversal method calls a pre_visit and a post_visit method and
traverses into any known internal data. Returning true from pre_visit and some
post_visit methods will skip the current data.

Traversal starts off like so:
```python
  traverse_file(Schema fileSchema)
    pre_visit_file
    traverse_nested_decls(fileSchema)
      pre_visit_nested_decls
      for (decl : schema.getProto().getNestedNodes())
        pre_visit_decl
        switch (schema::Node)
          STRUCT: traverse_struct_decl(schema, decl)
          ENUM: traverse_enum_decl(schema, decl)
          ...
        post_visit_decl
      post_visit_nested_decls
    post_visit_file
```
Look in generic.h for the listing of methods and the parameters they take, as
well as the traversal tree.


JSON
----

Included is a reference implementation of dealing with JSON. It uses rapidjson
to handle outputting with commas in the right place, and it helped in
development when DeathHandler gave stack traces for where the JSON would have
been malformed, but instead raised exceptions.



Requirements:
-------------

* DeathHandler: https://github.com/vmarkovtsev/DeathHandler
* rapidjson: https://github.com/miloyip/rapidjson
  * For the json reference implementation only.
