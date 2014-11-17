
#include <stdio.h>
#include <unistd.h>
#include <typeinfo>

#include <kj/main.h>
#include <kj/string.h>
#include <capnp/dynamic.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <capnp/schema.capnp.h>
#include <capnp/schema.h>
#include <capnp/schema-loader.h>

using namespace capnp;

#define GUARD_FALSE(result) if(result) return true
#define PRE_VISIT(type, ...) GUARD_FALSE(pre_visit_##type(__VA_ARGS__))
#define POST_VISIT(type, ...) GUARD_FALSE(post_visit_##type(__VA_ARGS__))
#define TRAVERSE(type, ...) traverse_##type(__VA_ARGS__)

// Use this to do something when the scope exits.
template<typename F>
class FinallyImpl {
 public:
  FinallyImpl(F f) : f_(f) {};
  ~FinallyImpl() { f_(); };
 private:
  F f_;
};
template<typename F>
FinallyImpl<F> Finally(F f) {
  return FinallyImpl<F>(f);
}

class BaseGenerator {
  public:
   BaseGenerator(SchemaLoader& schemaLoader, FILE* fd)
       : schemaLoader(schemaLoader), output(fd) {}
  SchemaLoader &schemaLoader;
  FILE *output;

  const static auto TRAVERSAL_LIMIT = 1 << 30;  // Don't limit.
  constexpr static const char *TITLE = "Generator title";
  constexpr static const char *DESCRIPTION = "Generator description";

  virtual bool traverse_file(Schema file, schema::CodeGeneratorRequest::RequestedFile::Reader requestedFile) {
    PRE_VISIT(file, file);
    TRAVERSE(nested_decls, file);
    TRAVERSE(imports, file, requestedFile.getImports());

    auto proto = file.getProto();
    TRAVERSE(annotations, file, proto.getAnnotations());
    POST_VISIT(file, file);
    return false;
  }

  virtual void finish() {}

  typedef schema::CodeGeneratorRequest::RequestedFile::Import Import;
  virtual bool traverse_imports(Schema schema, List<Import>::Reader imports) {
    PRE_VISIT(imports, schema, imports);
    for (auto import : imports) {
      PRE_VISIT(import, schema, import);
      POST_VISIT(import, schema, import);
    }
    POST_VISIT(imports, schema, imports);
    return false;
  }

  virtual bool traverse_nested_decls(Schema schema) {
    auto proto = schema.getProto();
    auto nodes = proto.getNestedNodes();
    if (nodes.size() == 0) return false;
    PRE_VISIT(nested_decls, schema);
    for (auto decl : nodes) {
      auto schema = schemaLoader.get(decl.getId());
      auto proto = schema.getProto();
      PRE_VISIT(decl, schema, decl);
      switch (proto.which()) {
        case schema::Node::FILE:
          break;
        case schema::Node::STRUCT: {
          TRAVERSE(struct_decl, schema, decl); break;
        }
        case schema::Node::ENUM: {
          TRAVERSE(enum_decl, schema, decl); break;
        }
        case schema::Node::INTERFACE: {
          TRAVERSE(interface_decl, schema, decl); break;
        }
        case schema::Node::CONST: {
          TRAVERSE(const_decl, schema, decl); break;
        }
        case schema::Node::ANNOTATION: {
          TRAVERSE(annotation_decl, schema, decl); break;
        }
      }
      POST_VISIT(decl, schema, decl);
    }
    POST_VISIT(nested_decls, schema);
    return false;
  }

  virtual bool traverse_struct_decl(Schema schema, schema::Node::NestedNode::Reader decl) {
    PRE_VISIT(struct_decl, schema, decl);
    TRAVERSE(annotations, schema);
    TRAVERSE(struct_fields, schema.asStruct());
    TRAVERSE(nested_decls, schema);
    POST_VISIT(struct_decl, schema, decl);
    return false;
  }

  virtual bool traverse_enum_decl(Schema schema, schema::Node::NestedNode::Reader decl) {
    PRE_VISIT(enum_decl, schema, decl);
    TRAVERSE(annotations, schema);
    TRAVERSE(enumerants, schema, schema.asEnum().getEnumerants());
    TRAVERSE(nested_decls, schema);
    POST_VISIT(enum_decl, schema, decl);
    return false;
  }

  virtual bool traverse_const_decl(Schema schema, schema::Node::NestedNode::Reader decl) {
    auto proto = schema.getProto();
    PRE_VISIT(const_decl, schema, decl);
    TRAVERSE(annotations, schema);
    TRAVERSE(type, schema, proto.getConst().getType());
    TRAVERSE(value, schema, proto.getConst().getType(), proto.getConst().getValue());
    POST_VISIT(const_decl, schema, decl);
    return false;
  }

  virtual bool traverse_annotation_decl(Schema schema, schema::Node::NestedNode::Reader decl ) {
    PRE_VISIT(annotation_decl, schema, decl);
    TRAVERSE(annotations, schema);
    TRAVERSE(type, schema, schema.getProto().getAnnotation().getType());
    POST_VISIT(annotation_decl, schema, decl);
    return false;
  }

  virtual bool traverse_annotations(Schema schema) {
    TRAVERSE(annotations, schema, schema.getProto().getAnnotations());
    return false;
  }

  virtual bool traverse_annotations(Schema schema, capnp::List<capnp::schema::Annotation>::Reader annotations) {
    if (annotations.size() == 0) return false;
    PRE_VISIT(annotations, schema);
    for (auto ann : annotations) {
      auto annSchema = schemaLoader.get(ann.getId(), ann.getBrand(), schema);
      TRAVERSE(annotation, ann, annSchema);
    }
    POST_VISIT(annotations, schema);
    return false;
  }

  virtual bool traverse_annotation(schema::Annotation::Reader annotation, Schema parent) {
    PRE_VISIT(annotation, annotation, parent);
    auto decl = schemaLoader.get(annotation.getId(), annotation.getBrand(), parent);
    auto annDecl = decl.getProto().getAnnotation();
    TRAVERSE(value, parent, annDecl.getType(), annotation.getValue());
    POST_VISIT(annotation, annotation, parent);
    return false;
  }

  virtual bool traverse_type(Schema& schema, schema::Type::Reader type) {
    PRE_VISIT(type, schema, type);
    POST_VISIT(type, schema, type);
    return false;
  }

  virtual bool traverse_value(Schema& schema, schema::Type::Reader type, schema::Value::Reader value) {
    PRE_VISIT(value, schema, type, value);
    switch (value.which()) {
      case schema::Value::VOID:
      case schema::Value::BOOL:
      case schema::Value::INT8:
      case schema::Value::INT16:
      case schema::Value::INT32:
      case schema::Value::INT64:
      case schema::Value::UINT8:
      case schema::Value::UINT16:
      case schema::Value::UINT32:
      case schema::Value::UINT64:
      case schema::Value::FLOAT32:
      case schema::Value::FLOAT64:
      case schema::Value::TEXT:
      case schema::Value::DATA:
        break;
      case schema::Value::LIST: {
        // auto listValue = value.getList().getAs<DynamicList>(
        //     ListSchema::of(type.getList().getElementType(), schema));
        break;
      }
      case schema::Value::ENUM: {
        // auto enumNode = schemaLoader.get(type.getEnum().getTypeId()).asEnum().getProto();
        // auto enumerants = enumNode.getEnum().getEnumerants();
        break;
      }
      case schema::Value::STRUCT: {
        // auto structValue = value.getStruct().getAs<DynamicStruct>(
        //     schemaLoader.get(type.getStruct().getTypeId()).asStruct());
        break;
      }
      case schema::Value::INTERFACE: {
        break;
      }
      case schema::Value::ANY_POINTER:
        break;
    }
    POST_VISIT(value, schema, type, value);
    return false;
  }

  virtual bool traverse_struct_fields(StructSchema schema) {
    PRE_VISIT(struct_fields, schema);
    for (auto field : schema.getFields()) {
      auto proto = field.getProto();
      PRE_VISIT(struct_field, schema, field);
      switch (proto.which()) {
        case schema::Field::SLOT: {
          auto slot = proto.getSlot();
          PRE_VISIT(struct_field_slot, schema, field, slot);
          TRAVERSE(type, schema, slot.getType());
          if (slot.getHadExplicitDefault()) {
            PRE_VISIT(struct_default_value, schema, field);
            TRAVERSE(value, schema, slot.getType(), slot.getDefaultValue());
            POST_VISIT(struct_default_value, schema, field);
          }
          POST_VISIT(struct_field_slot, schema, field, slot);
          break;
        }
        case schema::Field::GROUP: {
          auto group = proto.getGroup();
          auto groupSchema = schemaLoader.get(group.getTypeId());
          PRE_VISIT(struct_field_group, schema, field, group, groupSchema);
          TRAVERSE(struct_fields, groupSchema.asStruct());
          POST_VISIT(struct_field_group, schema, field, group, groupSchema);
          break;
        }
      }
      POST_VISIT(struct_field, schema, field);
    }
    POST_VISIT(struct_fields, schema);
    return false;
  }

  virtual bool traverse_interface_decl(Schema schema, schema::Node::NestedNode::Reader decl) {
    PRE_VISIT(interface_decl, schema, decl);
    TRAVERSE(annotations, schema);
    auto interface = schema.asInterface();
    PRE_VISIT(methods, interface);
    for (auto method : interface.getMethods()) {
      PRE_VISIT(method, interface, method);
      auto methodProto = method.getProto();
      TRAVERSE(annotations, schema, methodProto.getAnnotations());
      auto params = schemaLoader.get(
          methodProto.getParamStructType(),
          methodProto.getParamBrand(), schema).asStruct();
      auto results = schemaLoader.get(
          methodProto.getResultStructType(),
          methodProto.getResultBrand(), schema).asStruct();
      TRAVERSE(param_list, interface, kj::str("parameters"), params);
      TRAVERSE(param_list, interface, kj::str("results"), results);
      if (methodProto.hasImplicitParameters()) {
        auto implicit = methodProto.getImplicitParameters();
        PRE_VISIT(method_implicit_params, interface, method, implicit);
        POST_VISIT(method_implicit_params, interface, method, implicit);
      }
      POST_VISIT(method, interface, method);
    }
    POST_VISIT(methods, interface);
    TRAVERSE(nested_decls, schema);
    POST_VISIT(interface_decl, schema, decl);
    return false;
  }

  virtual bool traverse_param_list(InterfaceSchema interface, kj::String name, StructSchema schema) {
    PRE_VISIT(param_list, interface, name, schema);
    TRAVERSE(struct_fields, schema);
    POST_VISIT(param_list, interface, name, schema);
    return false;
  }

  virtual bool traverse_enumerants(Schema schema, EnumSchema::EnumerantList enumList) {
    PRE_VISIT(enumerants, schema, enumList);
    for (auto enumerant : enumList) {
      PRE_VISIT(enumerant, schema, enumerant);
      auto proto = enumerant.getProto();
      TRAVERSE(annotations, schema, proto.getAnnotations());
      POST_VISIT(enumerant, schema, enumerant);
    }
    POST_VISIT(enumerants, schema, enumList);
    return false;
  }

  #define VIRTUAL_BOOL_METHOD(func_name, ...) virtual bool func_name(__VA_ARGS__) { \
    return false; \
  }

  /*[[[cog
  def output_method(method, args):
    cog.outl('virtual bool %s(%s) { return false; }' % (method, ', '.join(args)))
  methods = {
    'file': ['Schema'],
    'imports': ['Schema', 'List<Import>::Reader'],
    'import': ['Schema', 'Import::Reader'],
    'nested_decls': ['Schema'],
    'decl': ['Schema', 'schema::Node::NestedNode::Reader'],
    'struct_decl': ['Schema', 'schema::Node::NestedNode::Reader'],
    'enum_decl': ['Schema', 'schema::Node::NestedNode::Reader'],
    'const_decl': ['Schema', 'schema::Node::NestedNode::Reader'],
    'annotation_decl': ['Schema', 'schema::Node::NestedNode::Reader'],
    'annotation': ['schema::Annotation::Reader', 'Schema'],
    'annotations': ['Schema'],
    'type': ['Schema', 'schema::Type::Reader'],
    'value': ['Schema', 'schema::Type::Reader', 'schema::Value::Reader'],
    'struct_fields': ['StructSchema'],
    'struct_default_value': ['StructSchema', 'capnp::StructSchema::Field'],
    'struct_field': ['StructSchema', 'StructSchema::Field'],
    'struct_field_slot': ['StructSchema', 'StructSchema::Field',
                          'schema::Field::Slot::Reader'],
    'struct_field_group': ['StructSchema', 'StructSchema::Field',
                           'schema::Field::Group::Reader', 'Schema'],
    'interface_decl': ['Schema', 'schema::Node::NestedNode::Reader'],
    'param_list': ['InterfaceSchema', 'kj::String&', 'StructSchema'],
    'method': ['InterfaceSchema', 'InterfaceSchema::Method'],
    'methods': ['InterfaceSchema'],
    'method_implicit_params': [
        'InterfaceSchema', 'InterfaceSchema::Method',
        'capnp::List<capnp::schema::Node::Parameter>::Reader'],
    'enumerant': ['Schema', 'EnumSchema::Enumerant'],
    'enumerants': ['Schema', 'EnumSchema::EnumerantList'],
  }
  for method, args in sorted(methods.items()):
    output_method('pre_visit_%s' % method, args)
    output_method('post_visit_%s' % method, args)
  ]]]*/
  virtual bool pre_visit_annotation(schema::Annotation::Reader, Schema) { return false; }
  virtual bool post_visit_annotation(schema::Annotation::Reader, Schema) { return false; }
  virtual bool pre_visit_annotation_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool post_visit_annotation_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool pre_visit_annotations(Schema) { return false; }
  virtual bool post_visit_annotations(Schema) { return false; }
  virtual bool pre_visit_const_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool post_visit_const_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool pre_visit_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool post_visit_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool pre_visit_enum_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool post_visit_enum_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool pre_visit_enumerant(Schema, EnumSchema::Enumerant) { return false; }
  virtual bool post_visit_enumerant(Schema, EnumSchema::Enumerant) { return false; }
  virtual bool pre_visit_enumerants(Schema, EnumSchema::EnumerantList) { return false; }
  virtual bool post_visit_enumerants(Schema, EnumSchema::EnumerantList) { return false; }
  virtual bool pre_visit_file(Schema) { return false; }
  virtual bool post_visit_file(Schema) { return false; }
  virtual bool pre_visit_import(Schema, Import::Reader) { return false; }
  virtual bool post_visit_import(Schema, Import::Reader) { return false; }
  virtual bool pre_visit_imports(Schema, List<Import>::Reader) { return false; }
  virtual bool post_visit_imports(Schema, List<Import>::Reader) { return false; }
  virtual bool pre_visit_interface_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool post_visit_interface_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool pre_visit_method(InterfaceSchema, InterfaceSchema::Method) { return false; }
  virtual bool post_visit_method(InterfaceSchema, InterfaceSchema::Method) { return false; }
  virtual bool pre_visit_method_implicit_params(InterfaceSchema, InterfaceSchema::Method, capnp::List<capnp::schema::Node::Parameter>::Reader) { return false; }
  virtual bool post_visit_method_implicit_params(InterfaceSchema, InterfaceSchema::Method, capnp::List<capnp::schema::Node::Parameter>::Reader) { return false; }
  virtual bool pre_visit_methods(InterfaceSchema) { return false; }
  virtual bool post_visit_methods(InterfaceSchema) { return false; }
  virtual bool pre_visit_nested_decls(Schema) { return false; }
  virtual bool post_visit_nested_decls(Schema) { return false; }
  virtual bool pre_visit_param_list(InterfaceSchema, kj::String&, StructSchema) { return false; }
  virtual bool post_visit_param_list(InterfaceSchema, kj::String&, StructSchema) { return false; }
  virtual bool pre_visit_struct_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool post_visit_struct_decl(Schema, schema::Node::NestedNode::Reader) { return false; }
  virtual bool pre_visit_struct_default_value(StructSchema, capnp::StructSchema::Field) { return false; }
  virtual bool post_visit_struct_default_value(StructSchema, capnp::StructSchema::Field) { return false; }
  virtual bool pre_visit_struct_field(StructSchema, StructSchema::Field) { return false; }
  virtual bool post_visit_struct_field(StructSchema, StructSchema::Field) { return false; }
  virtual bool pre_visit_struct_field_group(StructSchema, StructSchema::Field, schema::Field::Group::Reader, Schema) { return false; }
  virtual bool post_visit_struct_field_group(StructSchema, StructSchema::Field, schema::Field::Group::Reader, Schema) { return false; }
  virtual bool pre_visit_struct_field_slot(StructSchema, StructSchema::Field, schema::Field::Slot::Reader) { return false; }
  virtual bool post_visit_struct_field_slot(StructSchema, StructSchema::Field, schema::Field::Slot::Reader) { return false; }
  virtual bool pre_visit_struct_fields(StructSchema) { return false; }
  virtual bool post_visit_struct_fields(StructSchema) { return false; }
  virtual bool pre_visit_type(Schema, schema::Type::Reader) { return false; }
  virtual bool post_visit_type(Schema, schema::Type::Reader) { return false; }
  virtual bool pre_visit_value(Schema, schema::Type::Reader, schema::Value::Reader) { return false; }
  virtual bool post_visit_value(Schema, schema::Type::Reader, schema::Value::Reader) { return false; }
  //[[[end]]]
};


#ifndef VERSION
#define VERSION "(unknown)"
#endif

#include <signal.h>
#include "death_handler.h"

template <class Generator>
class CapnpcGenericMain {
 public:
  CapnpcGenericMain(kj::ProcessContext& context): context(context) {}

  kj::MainFunc getMain() {
    return kj::MainBuilder(context, Generator::TITLE, Generator::DESCRIPTION)
        .callAfterParsing(KJ_BIND_METHOD(*this, run))
        .build();
  }

 private:
  kj::ProcessContext& context;
  SchemaLoader schemaLoader;

  kj::MainBuilder::Validity run() {
    Debug::DeathHandler dh;
    ReaderOptions options;
    options.traversalLimitInWords = Generator::TRAVERSAL_LIMIT;
    StreamFdMessageReader reader(STDIN_FILENO, options);
    auto request = reader.getRoot<schema::CodeGeneratorRequest>();

    // Load the nodes first, we'll use them later.
    for (auto node: request.getNodes()) {
      schemaLoader.load(node);
    }

    Generator generator(schemaLoader, stdout);
    for (auto requestedFile: request.getRequestedFiles()) {
      auto schema = schemaLoader.get(requestedFile.getId());
      generator.traverse_file(schema, requestedFile);
    }
    generator.finish();
    fflush(stdout);

    return true;

  }

};


