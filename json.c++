#include "generic.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filestream.h"

class CapnpcJson : public BaseGenerator {
 public:
  CapnpcJson(SchemaLoader &schemaLoader, FILE *fd)
      : BaseGenerator(schemaLoader, fd), stream(fd), writer(stream) {
    writer.StartObject();
    writer.Key("files");
    writer.StartArray();
  }

  void finish() {
    writer.EndArray();
    writer.EndObject();
    stream.Put('\n');
  }

  const static auto TRAVERSAL_LIMIT = 1 << 30;  // Don't limit.
  constexpr static const char *TITLE = "JSON Generator";
  constexpr static const char *DESCRIPTION = "JSON Generator";

 private:
  rapidjson::FileStream stream;
  rapidjson::PrettyWriter<rapidjson::FileStream> writer;
  kj::String struct_field_reason_;
  kj::String value_reason_;
  constexpr static const char* default_type_reason_ = u8"type";
  const char* type_reason_ = default_type_reason_;

  bool pre_visit_file(Schema schema) override {
    auto proto = schema.getProto();
    writer.StartObject();
    writer.Key("id");
    writer.Uint64(proto.getId());
    writer.Key("name");
    writer.String(proto.getDisplayName().cStr());
    return false;
  }

  bool post_visit_file(Schema) override {
    writer.EndObject();
    return false;
  }

  bool pre_visit_nested_decls(Schema) override {
    writer.Key("nodes");
    writer.StartArray();
    return false;
  }

  bool post_visit_nested_decls(Schema) override {
    writer.EndArray();
    return false;
  }

  bool pre_visit_decl(Schema schema, schema::Node::NestedNode::Reader decl) {
    writer.StartObject();
    writer.Key("id");
    writer.Uint64(decl.getId());
    auto proto = schema.getProto();
    writer.Key("scopeId");
    writer.Uint64(proto.getScopeId());
    writer.Key("which");
    switch (proto.which()) {
      /*[[[cog
      types = ['struct', 'enum', 'interface', 'file', 'const', 'annotation'];
      for type in types:
        cog.outl('case schema::Node::%s:' % type.upper())
        cog.outl('  writer.String("%s");' % type.lower())
        cog.outl('  break;')
      ]]]*/
      case schema::Node::STRUCT:
        writer.String("struct");
        break;
      case schema::Node::ENUM:
        writer.String("enum");
        break;
      case schema::Node::INTERFACE:
        writer.String("interface");
        break;
      case schema::Node::FILE:
        writer.String("file");
        break;
      case schema::Node::CONST:
        writer.String("const");
        break;
      case schema::Node::ANNOTATION:
        writer.String("annotation");
        break;
      //[[[end]]]
      default:
        break;
    }
    writer.Key("name");
    writer.String(schema.getShortDisplayName().cStr());
    return false;
  }

  bool post_visit_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.EndObject();
    return false;
  }

  bool pre_visit_const_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.Key("const");
    writer.StartObject();
    value_reason_ = kj::str("value");
    return false;
  }

  bool post_visit_const_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.EndObject();
    return false;
  }

  bool pre_visit_enum_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.Key("enum");
    writer.StartObject();
    return false;
  }

  bool post_visit_enum_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.EndObject();
    return false;
  }

  bool pre_visit_enumerants(Schema, EnumSchema::EnumerantList) {
    writer.Key("enumerants");
    writer.StartArray();
    return false;
  }

  bool post_visit_enumerants(Schema, EnumSchema::EnumerantList) {
    writer.EndArray();
    return false;
  }

  bool pre_visit_enumerant(Schema, EnumSchema::Enumerant enumerant) {
    writer.StartObject();
    writer.Key("name");
    writer.String(enumerant.getProto().getName().cStr());
    writer.Key("ordinal");
    writer.Uint(enumerant.getOrdinal());
    return false;
  }

  bool post_visit_enumerant(Schema, EnumSchema::Enumerant) {
    writer.EndObject();
    return false;
  }

  virtual bool pre_visit_annotation_decl(Schema schema, schema::Node::NestedNode::Reader) override {
    auto proto = schema.getProto().getAnnotation();
    proto.getType();  // suppress warning.

    writer.Key("annotation");
    {
      writer.StartObject();
      writer.Key("targets");
      {
        writer.StartObject();
        /*[[[cog
        targets = [
            'struct', 'interface', 'group', 'enum', 'file', 'field', 'union',
            'group', 'enumerant', 'annotation', 'const', 'param', 'method',
        ]
        for target in targets:
          if smaller:
            cog.outl('if (proto.getTargets%s()) {' % target.title())
            cog.outl('  writer.Key("%s");' % target.lower())
            cog.outl('  writer.Bool(true);')
            cog.outl('}')
          else:
            cog.outl('writer.Key("%s");' % target.lower())
            cog.outl('writer.Bool(proto.getTargets%s());' % target.title())
        ]]]*/
        if (proto.getTargetsStruct()) {
          writer.Key("struct");
          writer.Bool(true);
        }
        if (proto.getTargetsInterface()) {
          writer.Key("interface");
          writer.Bool(true);
        }
        if (proto.getTargetsGroup()) {
          writer.Key("group");
          writer.Bool(true);
        }
        if (proto.getTargetsEnum()) {
          writer.Key("enum");
          writer.Bool(true);
        }
        if (proto.getTargetsFile()) {
          writer.Key("file");
          writer.Bool(true);
        }
        if (proto.getTargetsField()) {
          writer.Key("field");
          writer.Bool(true);
        }
        if (proto.getTargetsUnion()) {
          writer.Key("union");
          writer.Bool(true);
        }
        if (proto.getTargetsGroup()) {
          writer.Key("group");
          writer.Bool(true);
        }
        if (proto.getTargetsEnumerant()) {
          writer.Key("enumerant");
          writer.Bool(true);
        }
        if (proto.getTargetsAnnotation()) {
          writer.Key("annotation");
          writer.Bool(true);
        }
        if (proto.getTargetsConst()) {
          writer.Key("const");
          writer.Bool(true);
        }
        if (proto.getTargetsParam()) {
          writer.Key("param");
          writer.Bool(true);
        }
        if (proto.getTargetsMethod()) {
          writer.Key("method");
          writer.Bool(true);
        }
        //[[[end]]]
        writer.EndObject();
      }
    }
    return false;
  }

  virtual bool post_visit_annotation_decl(Schema, schema::Node::NestedNode::Reader) override {
    writer.EndObject();
    return false;
  }

  bool pre_visit_struct_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.Key("struct");
    writer.StartObject();
    struct_field_reason_ = kj::str("fields");
    return false;
  }

  bool post_visit_struct_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.EndObject();
    return false;
  }

  bool pre_visit_type(Schema schema, schema::Type::Reader type) {
    writer.Key(type_reason_);
    switch (type.which()) {
      /*[[[cog
      types = ['void', 'bool', 'text', 'data', 'float32', 'float64']
      types.extend('int%s' % size for size in [8, 16, 32, 64])
      types.extend('uint%s' % size for size in [8, 16, 32, 64])
      for type in types:
        cog.outl('case schema::Type::%s:' % type.upper())
        cog.outl('  writer.String("%s");' % type.lower())
        cog.outl('  break;')
      ]]]*/
      case schema::Type::VOID:
        writer.String("void");
        break;
      case schema::Type::BOOL:
        writer.String("bool");
        break;
      case schema::Type::TEXT:
        writer.String("text");
        break;
      case schema::Type::DATA:
        writer.String("data");
        break;
      case schema::Type::FLOAT32:
        writer.String("float32");
        break;
      case schema::Type::FLOAT64:
        writer.String("float64");
        break;
      case schema::Type::INT8:
        writer.String("int8");
        break;
      case schema::Type::INT16:
        writer.String("int16");
        break;
      case schema::Type::INT32:
        writer.String("int32");
        break;
      case schema::Type::INT64:
        writer.String("int64");
        break;
      case schema::Type::UINT8:
        writer.String("uint8");
        break;
      case schema::Type::UINT16:
        writer.String("uint16");
        break;
      case schema::Type::UINT32:
        writer.String("uint32");
        break;
      case schema::Type::UINT64:
        writer.String("uint64");
        break;
      //[[[end]]]
      case schema::Type::LIST: {
        writer.StartObject();
        writer.Key("which");
        writer.String("list");
        {
          type_reason_ = "elementType";
          auto _ = Finally([&](){type_reason_ = default_type_reason_;});

          pre_visit_type(schema, type.getList().getElementType());
        }
        writer.EndObject();
        break;
      }
      case schema::Type::ENUM: {
        auto enumSchema = schemaLoader.get(
            type.getEnum().getTypeId(), type.getEnum().getBrand(), schema);
        writer.StartObject();
        writer.Key("which");
        writer.String("enum");
        writer.Key("typeId");
        writer.Uint64(enumSchema.getProto().getId());
        writer.Key("name");
        writer.String(enumSchema.getShortDisplayName().cStr());
        writer.EndObject();
        break;
      }
      case schema::Type::STRUCT: {
        auto structSchema = schemaLoader.get(
            type.getStruct().getTypeId(), type.getStruct().getBrand(), schema);
        writer.StartObject();
        writer.Key("which");
        writer.String("struct");
        writer.Key("typeId");
        writer.Uint64(structSchema.getProto().getId());
        writer.Key("name");
        writer.String(structSchema.getShortDisplayName().cStr());
        writer.EndObject();
        break;
      }
      case schema::Type::INTERFACE: {
        auto ifaceSchema = schemaLoader.get(
            type.getInterface().getTypeId(), type.getInterface().getBrand(), schema);
        writer.StartObject();
        writer.Key("which");
        writer.String("interface");
        writer.Key("typeId");
        writer.Uint64(ifaceSchema.getProto().getId());
        writer.Key("name");
        writer.String(ifaceSchema.getShortDisplayName().cStr());
        writer.EndObject();
        break;
      }
      case schema::Type::ANY_POINTER:
        writer.StartObject();
        writer.Key("which");
        writer.String("anypointer");
        writer.Key("unconstrained");
        writer.Bool(type.getAnyPointer().isUnconstrained());
        writer.EndObject();
        break;
    }
    return false;
  }

  bool pre_visit_value(Schema schema, schema::Type::Reader type, schema::Value::Reader value) {
    writer.Key(value_reason_.cStr());
    value_reason_ = kj::str("ERROR");
    visit_value_w_type(schema, schemaLoader.getType(type, schema), value);
    return false;
  }

  void visit_value_dynamic(Schema schema, Type type, DynamicValue::Reader value) {
    // Sadly, this almost-exact-copy of visit_value_w_type is needed to deal
    // with the fact that you can't get a 'concrete' schema::Value out of a
    // List or its ilk.
    switch (type.which()) {
      /*[[[cog
      sizes32 = [8, 16, 32]
      sizes64 = [64]
      types = [
          ('bool', 'bool', 'bool'),
          ('int64', 'int64_t', 'int64'),
          ('uint64', 'uint64_t', 'uint64'),
          ('float32', 'float', 'double'),
          ('float64', 'double', 'double')
      ] + [
          ('int%d' % size, 'int%d_t' % size, 'int') for size in sizes32
      ] + [
          ('uint%d' % size, 'uint%d_t' % size, 'uint') for size in sizes32
      ] 
      for type, ctype, writer in types:
        cog.outl('case schema::Type::%s:' % type.upper())
        cog.outl('  writer.%s(value.as<%s>());' % (writer.title(), ctype))
        cog.outl('  break;')
      ]]]*/
      case schema::Type::BOOL:
        writer.Bool(value.as<bool>());
        break;
      case schema::Type::INT64:
        writer.Int64(value.as<int64_t>());
        break;
      case schema::Type::UINT64:
        writer.Uint64(value.as<uint64_t>());
        break;
      case schema::Type::FLOAT32:
        writer.Double(value.as<float>());
        break;
      case schema::Type::FLOAT64:
        writer.Double(value.as<double>());
        break;
      case schema::Type::INT8:
        writer.Int(value.as<int8_t>());
        break;
      case schema::Type::INT16:
        writer.Int(value.as<int16_t>());
        break;
      case schema::Type::INT32:
        writer.Int(value.as<int32_t>());
        break;
      case schema::Type::UINT8:
        writer.Uint(value.as<uint8_t>());
        break;
      case schema::Type::UINT16:
        writer.Uint(value.as<uint16_t>());
        break;
      case schema::Type::UINT32:
        writer.Uint(value.as<uint32_t>());
        break;
      //[[[end]]]
      case schema::Type::VOID:
        writer.String("void"); break;
      case schema::Type::TEXT:
        writer.String(value.as<Text>().cStr()); break;
      case schema::Type::DATA:
        writer.StartArray();
        for (auto data : value.as<Data>()) {
          writer.String(reinterpret_cast<const char*>(&data), 1);
        }
        writer.EndArray();
        break;
      case schema::Type::LIST: {
        writer.StartArray();
        auto listType = type.asList();
        auto listValue = value.as<DynamicList>();
        for (auto element : listValue) {
          visit_value_dynamic(schema, listType.getElementType(), element);
        }
        writer.EndArray();
        break;
      }
      case schema::Type::ENUM: {
        auto enumValue = value.as<DynamicEnum>();
        writer.StartObject();
        writer.Key("value");
        writer.Uint(enumValue.getRaw());
        KJ_IF_MAYBE(enumerant, enumValue.getEnumerant()) {
          writer.Key("enumerant");
          writer.String(enumerant->getProto().getName().cStr());
        }
        writer.EndObject();
        break;
      }
      case schema::Type::STRUCT: {
        auto structValue = value.as<DynamicStruct>();
        writer.StartObject();
        for (auto field : type.asStruct().getFields()) {
          if (structValue.has(field)) {
            writer.Key(field.getProto().getName().cStr());
            auto fieldValue = structValue.get(field);
            visit_value_dynamic(schema, field.getType(), fieldValue);
          }
        }
        writer.EndObject();
        break;
      }
      case schema::Type::INTERFACE: {
        writer.String(
            "interface? but that's not possible... how do you serialize an "
            "interface in a capnp file?");
        break;
      }
      case schema::Type::ANY_POINTER:
        writer.String(
            "any pointer? how do you serialize an anypointer in a capnp file");
        break;
    }
  }

  void visit_value_w_type(Schema schema, Type type, schema::Value::Reader value) {
    switch (type.which()) {
      /*[[[cog
      sizes32 = [8, 16, 32]
      sizes64 = [64]
      types = {'bool': 'bool',
               'int64': 'int64', 'uint64': 'uint64',
               'float32': 'double', 'float64': 'double'}
      types.update({'int%d' % size: 'int' for size in sizes32})
      types.update({'uint%d' % size: 'uint' for size in sizes32})
      for type, writer in sorted(types.items()):
        cog.outl('case schema::Type::%s:' % type.upper())
        cog.outl('  writer.%s(value.get%s());' % (writer.title(), type.title()))
        cog.outl('  break;')
      ]]]*/
      case schema::Type::BOOL:
        writer.Bool(value.getBool());
        break;
      case schema::Type::FLOAT32:
        writer.Double(value.getFloat32());
        break;
      case schema::Type::FLOAT64:
        writer.Double(value.getFloat64());
        break;
      case schema::Type::INT16:
        writer.Int(value.getInt16());
        break;
      case schema::Type::INT32:
        writer.Int(value.getInt32());
        break;
      case schema::Type::INT64:
        writer.Int64(value.getInt64());
        break;
      case schema::Type::INT8:
        writer.Int(value.getInt8());
        break;
      case schema::Type::UINT16:
        writer.Uint(value.getUint16());
        break;
      case schema::Type::UINT32:
        writer.Uint(value.getUint32());
        break;
      case schema::Type::UINT64:
        writer.Uint64(value.getUint64());
        break;
      case schema::Type::UINT8:
        writer.Uint(value.getUint8());
        break;
      //[[[end]]]
      case schema::Type::VOID:
        writer.String("void"); break;
      case schema::Type::TEXT:
        writer.String(value.getText().cStr()); break;
      case schema::Type::DATA:
        writer.StartArray();
        for (auto data : value.getData()) {
          writer.String(reinterpret_cast<const char*>(&data), 1);
        }
        writer.EndArray();
        break;
      case schema::Type::LIST: {
        writer.StartArray();
        auto listType = type.asList();
        auto listValue = value.getList().getAs<DynamicList>(listType);
        for (auto element : listValue) {
          visit_value_dynamic(schema, listType.getElementType(), element);
        }
        writer.EndArray();
        break;
      }
      case schema::Type::ENUM: {
        auto enumerants = type.asEnum().getEnumerants();
        writer.StartObject();
        writer.Key("value");
        writer.Uint(value.getEnum());
        writer.Key("enumerant");
        for (auto enumerant : enumerants) {
          if (enumerant.getIndex() == value.getEnum()) {
            writer.String(enumerant.getProto().getName().cStr());
          }
        }
        writer.EndObject();
        break;
      }
      case schema::Type::STRUCT: {
        auto structValue = value.getStruct().getAs<DynamicStruct>(type.asStruct());
        writer.StartObject();
        for (auto field : type.asStruct().getFields()) {
          if (structValue.has(field)) {
            writer.Key(field.getProto().getName().cStr());
            auto fieldValue = structValue.get(field);
            visit_value_dynamic(schema, field.getType(), fieldValue);
          }
        }
        writer.EndObject();
        break;
      }
      case schema::Type::INTERFACE: {
        writer.String(
            "interface? but that's not possible... how do you serialize an "
            "interface in a capnp file?");
        break;
      }
      case schema::Type::ANY_POINTER: {
        writer.String(
            "any pointer? how do you serialize an anypointer in a capnp file");
        break;
      }
    }
  }

  bool pre_visit_struct_fields(StructSchema) {
    writer.Key(struct_field_reason_.cStr());
    writer.StartArray();
    return false;
  }

  bool post_visit_struct_fields(StructSchema) {
    writer.EndArray();
    return false;
  }

  bool pre_visit_struct_field(StructSchema, StructSchema::Field field) {
    auto proto = field.getProto();
    writer.StartObject();
    writer.Key("name");
    writer.String(proto.getName().cStr());
    writer.Key("ordinal");
    writer.StartObject();
    auto ord = field.getProto().getOrdinal();
    if (ord.isExplicit()) {
      writer.Key("explicit");
      writer.Int(ord.getExplicit());
    } else {
      writer.Key("implicit");
      writer.Null();
    }
    writer.EndObject();
    return false;
  }

  bool pre_visit_struct_field_slot(StructSchema, StructSchema::Field, schema::Field::Slot::Reader slot) {
    writer.Key("offset");
    writer.Uint(slot.getOffset());
    writer.Key("hadDefaultValue");
    writer.Bool(slot.getHadExplicitDefault());
    return false;
  }

  bool pre_visit_struct_default_value(StructSchema, StructSchema::Field) {
    value_reason_ = kj::str("default");
    return false;
  }

  bool post_visit_struct_field(StructSchema, StructSchema::Field) {
    writer.EndObject();
    return false;
  }

  bool pre_visit_interface_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.Key("interface");
    writer.StartObject();
    return false;
  }

  bool post_visit_interface_decl(Schema, schema::Node::NestedNode::Reader) {
    writer.EndObject();
    return false;
  }

  bool pre_visit_param_list(InterfaceSchema, kj::String& name, StructSchema) {
    struct_field_reason_ = kj::str(name);
    return false;
  }

  bool post_visit_param_list(InterfaceSchema, kj::String&, StructSchema) {
    return false;
  }

  bool pre_visit_methods(InterfaceSchema) {
    writer.Key("methods");
    writer.StartArray();
    return false;
  }

  bool post_visit_methods(InterfaceSchema) {
    writer.EndArray();
    return false;
  }

  bool pre_visit_method(InterfaceSchema, InterfaceSchema::Method method) {
    writer.StartObject();
    writer.Key("name");
    writer.String(method.getProto().getName().cStr());
    writer.Key("ordinal");
    writer.Int(method.getOrdinal());
    return false;
  }

  bool post_visit_method(InterfaceSchema, InterfaceSchema::Method) {
    writer.EndObject();
    return false;
  }

  bool pre_visit_method_implicit_params(InterfaceSchema,
      InterfaceSchema::Method,
      capnp::List<capnp::schema::Node::Parameter>::Reader params) {
    writer.Key("implicit_parameters");
    writer.StartArray();
    for (auto param : params) {
      writer.String(param.getName().cStr());
    }
    writer.EndArray();
    return false;
  }

  bool pre_visit_annotations(Schema) {
    writer.Key("annotations");
    writer.StartArray();
    return false;
  }

  bool post_visit_annotations(Schema) {
    writer.EndArray();
    return false;
  }

  bool pre_visit_annotation(schema::Annotation::Reader annotation, Schema schema) {
    writer.StartObject();
    writer.Key("id");
    writer.Uint64(annotation.getId());
    writer.Key("name");
    auto decl = schemaLoader.get(annotation.getId(), annotation.getBrand(), schema);
    writer.String(decl.getShortDisplayName().cStr());
    value_reason_ = kj::str("value");
    return false;
  }

  bool post_visit_annotation(schema::Annotation::Reader, Schema) {
    writer.EndObject();
    return false;
  }
};


KJ_MAIN(CapnpcGenericMain<CapnpcJson>);
