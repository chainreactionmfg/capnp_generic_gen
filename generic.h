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
   BaseGenerator(SchemaLoader& schemaLoader)
       : schemaLoader(schemaLoader) {}
  SchemaLoader &schemaLoader;

  const static auto TRAVERSAL_LIMIT = 1 << 30;  // Don't limit.
  constexpr static const char *TITLE = "Generator title";
  constexpr static const char *DESCRIPTION = "Generator description";

  typedef schema::CodeGeneratorRequest::RequestedFile::Reader RequestedFile;
  virtual bool traverse_file(
      const Schema& file, const RequestedFile& requestedFile) {
    PRE_VISIT(file, file, requestedFile);
    TRAVERSE(imports, file, requestedFile.getImports());
    auto proto = file.getProto();
    TRAVERSE(nested_decls, file);
    TRAVERSE(annotations, file, proto.getAnnotations());
    POST_VISIT(file, file, requestedFile);
    return false;
  }

  virtual void finish() {}

  typedef schema::CodeGeneratorRequest::RequestedFile::Import Import;
  virtual bool traverse_imports(const Schema& schema, const List<Import>::Reader& imports) {
    PRE_VISIT(imports, schema, imports);
    for (auto import : imports) {
      PRE_VISIT(import, schema, import);
      POST_VISIT(import, schema, import);
    }
    POST_VISIT(imports, schema, imports);
    return false;
  }

  virtual bool traverse_nested_decls(const Schema& schema) {
    auto proto = schema.getProto();
    auto nodes = proto.getNestedNodes();
    if (nodes.size() == 0) return false;
    PRE_VISIT(nested_decls, schema);
    for (auto decl : nodes) {
      auto schema = schemaLoader.getUnbound(decl.getId());
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

  typedef schema::Node::NestedNode::Reader NestedNode;
  virtual bool traverse_struct_decl(const Schema& schema, const NestedNode& decl) {
    PRE_VISIT(struct_decl, schema, decl);
    TRAVERSE(nested_decls, schema);
    TRAVERSE(struct_fields, schema.asStruct());
    TRAVERSE(annotations, schema);
    POST_VISIT(struct_decl, schema, decl);
    return false;
  }

  virtual bool traverse_enum_decl(const Schema& schema, const NestedNode& decl) {
    PRE_VISIT(enum_decl, schema, decl);
    TRAVERSE(nested_decls, schema);
    TRAVERSE(enumerants, schema, schema.asEnum().getEnumerants());
    TRAVERSE(annotations, schema);
    POST_VISIT(enum_decl, schema, decl);
    return false;
  }

  virtual bool traverse_const_decl(const Schema& schema, const NestedNode& decl) {
    auto proto = schema.getProto();
    PRE_VISIT(const_decl, schema, decl);
    TRAVERSE(type, schema, proto.getConst().getType());
    TRAVERSE(value, schema, proto.getConst().getType(), proto.getConst().getValue());
    TRAVERSE(annotations, schema);
    POST_VISIT(const_decl, schema, decl);
    return false;
  }

  virtual bool traverse_annotation_decl(const Schema& schema, const NestedNode& decl ) {
    PRE_VISIT(annotation_decl, schema, decl);
    TRAVERSE(type, schema, schema.getProto().getAnnotation().getType());
    TRAVERSE(annotations, schema);
    POST_VISIT(annotation_decl, schema, decl);
    return false;
  }

  virtual bool traverse_annotations(const Schema& schema) {
    TRAVERSE(annotations, schema, schema.getProto().getAnnotations());
    return false;
  }

  virtual bool traverse_annotations(
      const Schema& schema, const List<schema::Annotation>::Reader& annotations) {
    if (annotations.size() == 0) return false;
    PRE_VISIT(annotations, schema);
    for (auto ann : annotations) {
      auto annSchema = schemaLoader.get(ann.getId(), ann.getBrand(), schema);
      TRAVERSE(annotation, ann, annSchema);
    }
    POST_VISIT(annotations, schema);
    return false;
  }

  virtual bool traverse_annotation(const schema::Annotation::Reader& annotation, const Schema& parent) {
    PRE_VISIT(annotation, annotation, parent);
    auto decl = schemaLoader.get(annotation.getId(), annotation.getBrand(), parent);
    auto annDecl = decl.getProto().getAnnotation();
    TRAVERSE(value, parent, annDecl.getType(), annotation.getValue());
    POST_VISIT(annotation, annotation, parent);
    return false;
  }

  virtual bool traverse_type(const Schema& schema, const schema::Type::Reader& type) {
    PRE_VISIT(type, schema, type);
    if (type.which() == schema::Type::LIST) {
      TRAVERSE(type, schema, type.getList().getElementType());
    }
    POST_VISIT(type, schema, type);
    return false;
  }

  virtual bool traverse_dynamic_value(const Schema& schema, const Type& type, const DynamicValue::Reader& value) {
    PRE_VISIT(dynamic_value, schema, type, value);
    switch (type.which()) {
      case schema::Type::LIST: {
        auto listType = type.asList();
        for (auto element : value.as<DynamicList>()) {
          TRAVERSE(dynamic_value, schema, listType.getElementType(), element);
        }
        break;
      }
      case schema::Type::STRUCT: {
        auto structValue = value.as<DynamicStruct>();
        for (auto field : type.asStruct().getFields()) {
          if (structValue.has(field)) {
            auto fieldValue = structValue.get(field);
            TRAVERSE(dynamic_value, schema, field.getType(), fieldValue);
          }
        }
        break;
      }
      default: break;
    }
    POST_VISIT(dynamic_value, schema, type, value);
    return false;
  }

  inline bool traverse_value(const Schema& schema, const schema::Type::Reader& type, const schema::Value::Reader& value) {
    return traverse_value(schema, schemaLoader.getType(type, schema), value);
  }

  virtual bool traverse_value(const Schema& schema, const Type& type, const schema::Value::Reader& value) {
    switch (value.which()) {
      /*[[[cog
      sizes = [8, 16, 32, 64]
      types = ['void', 'text', 'data', 'float32', 'float64', 'bool'] + [
          'int%d' % size for size in sizes] + [
          'uint%d' % size for size in sizes]
      for type in types:
        cog.outl('case schema::Value::%s:' % type.upper())
        cog.outl('  TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.get%s()));' % type.title())
        cog.outl('  break;')
      ]]]*/
      case schema::Value::VOID:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getVoid()));
        break;
      case schema::Value::TEXT:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getText()));
        break;
      case schema::Value::DATA:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getData()));
        break;
      case schema::Value::FLOAT32:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getFloat32()));
        break;
      case schema::Value::FLOAT64:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getFloat64()));
        break;
      case schema::Value::BOOL:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getBool()));
        break;
      case schema::Value::INT8:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getInt8()));
        break;
      case schema::Value::INT16:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getInt16()));
        break;
      case schema::Value::INT32:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getInt32()));
        break;
      case schema::Value::INT64:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getInt64()));
        break;
      case schema::Value::UINT8:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getUint8()));
        break;
      case schema::Value::UINT16:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getUint16()));
        break;
      case schema::Value::UINT32:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getUint32()));
        break;
      case schema::Value::UINT64:
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(value.getUint64()));
        break;
      //[[[end]]]
      case schema::Value::LIST: {
        auto listValue = value.getList().getAs<DynamicList>(type.asList());
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(listValue));
        break;
      }
      case schema::Value::ENUM: {
        auto dynamicEnum = DynamicEnum(type.asEnum(), value.getEnum());
        TRAVERSE(dynamic_value, schema, type, DynamicValue::Reader(dynamicEnum));
        break;
      }
      case schema::Value::STRUCT: {
        TRAVERSE(dynamic_value, schema, type, value.getStruct().getAs<DynamicStruct>(type.asStruct()));
        break;
      }
      case schema::Value::INTERFACE:
      case schema::Value::ANY_POINTER:
        // These cannot be serialized in a schema file.
        break;
    }
    return false;
  }

  virtual bool traverse_struct_fields(const StructSchema& schema) {
    PRE_VISIT(struct_fields, schema);
    for (auto field : schema.getFields()) {
      TRAVERSE(struct_field, schema, field);
    }
    POST_VISIT(struct_fields, schema);
    return false;
  }

  virtual bool traverse_struct_field(const StructSchema& schema, const StructSchema::Field& field) {
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
        auto groupSchema = schemaLoader.getUnbound(group.getTypeId());
        PRE_VISIT(struct_field_group, schema, field, group, groupSchema);
        TRAVERSE(struct_fields, groupSchema.asStruct());
        POST_VISIT(struct_field_group, schema, field, group, groupSchema);
        break;
      }
    }
    TRAVERSE(annotations, schema, proto.getAnnotations());
    POST_VISIT(struct_field, schema, field);
    return false;
  }

  virtual bool traverse_interface_decl(const Schema& schema, const NestedNode& decl) {
    auto interface = schema.asInterface();
    PRE_VISIT(interface_decl, schema, decl);
    TRAVERSE(nested_decls, schema);
    PRE_VISIT(methods, interface);
    for (auto method : interface.getMethods()) {
      TRAVERSE(method, interface, method);
    }
    POST_VISIT(methods, interface);
    TRAVERSE(annotations, schema);
    POST_VISIT(interface_decl, schema, decl);
    return false;
  }

  virtual bool traverse_method(const Schema& schema, const InterfaceSchema::Method& method) {
    auto interface = schema.asInterface();
    PRE_VISIT(method, interface, method);
    auto methodProto = method.getProto();
    if (methodProto.hasImplicitParameters()) {
      auto implicit = methodProto.getImplicitParameters();
      PRE_VISIT(method_implicit_params, interface, method, implicit);
      TRAVERSE(param_list, interface, kj::str("parameters"), schemaLoader.getUnbound(methodProto.getParamStructType()).asStruct());
      TRAVERSE(param_list, interface, kj::str("results"), schemaLoader.getUnbound(methodProto.getResultStructType()).asStruct());
      POST_VISIT(method_implicit_params, interface, method, implicit);
    } else {
      TRAVERSE(param_list, interface, kj::str("parameters"), method.getParamType());
      TRAVERSE(param_list, interface, kj::str("results"), method.getResultType());
    }
    TRAVERSE(annotations, schema, methodProto.getAnnotations());
    POST_VISIT(method, interface, method);
    return false;
  }

  virtual bool traverse_param_list(const InterfaceSchema& interface, const kj::String& name, const StructSchema& schema) {
    PRE_VISIT(param_list, interface, name, schema);
    TRAVERSE(struct_fields, schema);
    POST_VISIT(param_list, interface, name, schema);
    return false;
  }

  virtual bool traverse_enumerants(const Schema& schema, const EnumSchema::EnumerantList& enumList) {
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

  /*[[[cog
  def output_method(method, args):
    cog.outl('virtual bool %s(const %s&) { return false; }' % (
        method, '&, const '.join(args)))
  for method, args in visit_methods.items():
    output_method('pre_visit_%s' % method, args)
  for method, args in visit_methods.items():
    output_method('post_visit_%s' % method, args)
  ]]]*/
  virtual bool pre_visit_file(const Schema&, const schema::CodeGeneratorRequest::RequestedFile::Reader&) { return false; }
  virtual bool pre_visit_imports(const Schema&, const List<Import>::Reader&) { return false; }
  virtual bool pre_visit_import(const Schema&, const Import::Reader&) { return false; }
  virtual bool pre_visit_nested_decls(const Schema&) { return false; }
  virtual bool pre_visit_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool pre_visit_struct_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool pre_visit_enum_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool pre_visit_const_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool pre_visit_annotation_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool pre_visit_annotation(const schema::Annotation::Reader&, const Schema&) { return false; }
  virtual bool pre_visit_annotations(const Schema&) { return false; }
  virtual bool pre_visit_type(const Schema&, const schema::Type::Reader&) { return false; }
  virtual bool pre_visit_dynamic_value(const Schema&, const Type&, const DynamicValue::Reader&) { return false; }
  virtual bool pre_visit_struct_fields(const StructSchema&) { return false; }
  virtual bool pre_visit_struct_default_value(const StructSchema&, const StructSchema::Field&) { return false; }
  virtual bool pre_visit_struct_field(const StructSchema&, const StructSchema::Field&) { return false; }
  virtual bool pre_visit_struct_field_slot(const StructSchema&, const StructSchema::Field&, const schema::Field::Slot::Reader&) { return false; }
  virtual bool pre_visit_struct_field_group(const StructSchema&, const StructSchema::Field&, const schema::Field::Group::Reader&, const Schema&) { return false; }
  virtual bool pre_visit_interface_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool pre_visit_param_list(const InterfaceSchema&, const kj::String&, const StructSchema&) { return false; }
  virtual bool pre_visit_method(const InterfaceSchema&, const InterfaceSchema::Method&) { return false; }
  virtual bool pre_visit_methods(const InterfaceSchema&) { return false; }
  virtual bool pre_visit_method_implicit_params(const InterfaceSchema&, const InterfaceSchema::Method&, const List<schema::Node::Parameter>::Reader&) { return false; }
  virtual bool pre_visit_enumerant(const Schema&, const EnumSchema::Enumerant&) { return false; }
  virtual bool pre_visit_enumerants(const Schema&, const EnumSchema::EnumerantList&) { return false; }
  virtual bool post_visit_file(const Schema&, const schema::CodeGeneratorRequest::RequestedFile::Reader&) { return false; }
  virtual bool post_visit_imports(const Schema&, const List<Import>::Reader&) { return false; }
  virtual bool post_visit_import(const Schema&, const Import::Reader&) { return false; }
  virtual bool post_visit_nested_decls(const Schema&) { return false; }
  virtual bool post_visit_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool post_visit_struct_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool post_visit_enum_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool post_visit_const_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool post_visit_annotation_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool post_visit_annotation(const schema::Annotation::Reader&, const Schema&) { return false; }
  virtual bool post_visit_annotations(const Schema&) { return false; }
  virtual bool post_visit_type(const Schema&, const schema::Type::Reader&) { return false; }
  virtual bool post_visit_dynamic_value(const Schema&, const Type&, const DynamicValue::Reader&) { return false; }
  virtual bool post_visit_struct_fields(const StructSchema&) { return false; }
  virtual bool post_visit_struct_default_value(const StructSchema&, const StructSchema::Field&) { return false; }
  virtual bool post_visit_struct_field(const StructSchema&, const StructSchema::Field&) { return false; }
  virtual bool post_visit_struct_field_slot(const StructSchema&, const StructSchema::Field&, const schema::Field::Slot::Reader&) { return false; }
  virtual bool post_visit_struct_field_group(const StructSchema&, const StructSchema::Field&, const schema::Field::Group::Reader&, const Schema&) { return false; }
  virtual bool post_visit_interface_decl(const Schema&, const schema::Node::NestedNode::Reader&) { return false; }
  virtual bool post_visit_param_list(const InterfaceSchema&, const kj::String&, const StructSchema&) { return false; }
  virtual bool post_visit_method(const InterfaceSchema&, const InterfaceSchema::Method&) { return false; }
  virtual bool post_visit_methods(const InterfaceSchema&) { return false; }
  virtual bool post_visit_method_implicit_params(const InterfaceSchema&, const InterfaceSchema::Method&, const List<schema::Node::Parameter>::Reader&) { return false; }
  virtual bool post_visit_enumerant(const Schema&, const EnumSchema::Enumerant&) { return false; }
  virtual bool post_visit_enumerants(const Schema&, const EnumSchema::EnumerantList&) { return false; }
  //[[[end]]]
};


#ifndef VERSION
#define VERSION "(unknown)"
#endif

#include <signal.h>
#if USE_DEATH_HANDLER
#include "death_handler.h"
#endif

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
#if USE_DEATH_HANDLER
    Debug::DeathHandler dh;
#endif
    ReaderOptions options;
    options.traversalLimitInWords = Generator::TRAVERSAL_LIMIT;
    StreamFdMessageReader reader(STDIN_FILENO, options);
    auto request = reader.getRoot<schema::CodeGeneratorRequest>();

    // Load the nodes first, we'll use them later.
    for (auto node: request.getNodes()) {
      schemaLoader.load(node);
    }

    Generator generator(schemaLoader);
    for (auto requestedFile: request.getRequestedFiles()) {
      auto schema = schemaLoader.get(requestedFile.getId());
      generator.traverse_file(schema, requestedFile);
    }
    generator.finish();
    fflush(stdout);

    return true;

  }

};


