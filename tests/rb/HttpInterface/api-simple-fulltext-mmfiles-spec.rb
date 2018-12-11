# coding: utf-8

require 'rspec'
require 'arangodb.rb'

describe ArangoDB do
  api = "/_api/simple"
  prefix = "api-simple"

  context "simple queries:" do

################################################################################
## fulltext query
################################################################################

    context "fulltext query:" do
      before do
        @cn = "UnitTestsCollectionFulltext"
        ArangoDB.drop_collection(@cn)

        body = "{ \"name\" : \"#{@cn}\", \"numberOfShards\" : 8 }"
        doc = ArangoDB.post("/_api/collection", :body => body)
        doc.code.should eq(200)
        @cid = doc.parsed_response['id']

        texts = [
          "Nuevo. Ella ni caso a esa señora pagó leer Invitación amistad viajando comer todo lo que el a dos. Shy ustedes que apenas gastos debatiendo apresuró resuelto. Siempre educado momento en que es espíritu calor a los corazones. Downs esos ingeniosos aún un jefe bolas tan así. Momento un poco hasta quedarse sin ninguna animado. Camino de mayo trajo a nuestro país periódicas para adaptarse vitorearon.",
          "Él había enviado invitación bullicioso conexión habitar proyección. Por mutuo un peligro desván mr edward una. Desviado como adición esfuerzo estrictamente ninguna disposición por Stanhill. Esta mujer llamada lo hacen suspirar puerta no sentía. Usted y el orden morada pesar conseguirlo. La adquisición de lejos nuestra pertenencia a nosotros mismos y ciertamente propio perpetuo continuo. Es otra parte de mi a veces o certeza. Lain no como cinco o menos alto. Todo viajar establecer cómo la literatura ley.",
          "Se trató de chistes en bolsa china decaimiento listo un archivo. Pequeño su timidez tenía leñosa poder downs. Para que denota habla admitió aprendiendo mi ejercicio para Adquiridos pulg persianas mr lo sentimientos. Para o tres casa oferta tomado am a comenzar. Como disuadir alegre superó así de amable se entregaba sin envasar. Alteración de conexión así como me coleccionismo. Difícil entregado en extenso en subsidio dirección. Alteración poner disminución uso puede considerarse sentimientos discreción interesado. Un viendo débilmente escaleras soy yo sin ingresos rama.",
          "Contento obtener certeza desconfía más aún son jamón franqueza oculta. En la resolución no afecta a considerar de. No me pareció marido o coronel efectos formando. Shewing End sentado que vio además de musical adaptado hijo. En contraste interesados comer pianoforte alteración simpatizar fue. Él cree que si las familias no sorprender a un interés elegancia. Reposó millas equivocadas una placa tan demora. Ella puso propia relación sobrevivió podrá eliminarse."
        ]

        # insert documents
        texts.each do |text|
          ArangoDB.post("/_api/document?collection=#{@cn}", :body => "{ \"text\" : \"#{text}\" }")
        end
      end

      after do
        ArangoDB.drop_collection(@cn)
      end
      
      it "returns an error for fulltext query without query attribute" do
        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\" }"
        doc = ArangoDB.log_put("#{prefix}-fulltext-missing", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
      end

      it "returns an error for fulltext query without an index" do
        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\", \"query\" : \"foo\" }"
        doc = ArangoDB.log_put("#{prefix}-fulltext-missing", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1571)
      end
      
      it "returns an error for fulltext query with a wrong index" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"fulltext\", \"fields\" : [ \"text\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

              # query index on different attribute
        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"foo\", \"query\" : \"bar\" }"
        doc = ArangoDB.log_put("#{prefix}-fulltext-missing", cmd, :body => body)

        doc.code.should eq(400)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(true)
        doc.parsed_response['code'].should eq(400)
        doc.parsed_response['errorNum'].should eq(1571)
      end

      it "returns documents for fulltext queries" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"fulltext\", \"fields\" : [ \"text\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\", \"query\" : \"como,interesado,viendo\" }"
        doc = ArangoDB.log_put("#{prefix}-within", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(1)
        doc.parsed_response['count'].should eq(1)
      end
      
      it "returns documents for fulltext queries" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"fulltext\", \"fields\" : [ \"text\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\", \"query\" : \"que\", \"limit\" : 2 }"
        doc = ArangoDB.log_put("#{prefix}-within", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(2)
        doc.parsed_response['count'].should eq(2)
      end
      
      it "returns documents for fulltext queries" do
        # create index
        cmd = "/_api/index?collection=#{@cn}"
        body = "{ \"type\" : \"fulltext\", \"fields\" : [ \"text\" ] }"
        doc = ArangoDB.post(cmd, :body => body)

        cmd = api + "/fulltext"
        body = "{ \"collection\" : \"#{@cn}\", \"attribute\" : \"text\", \"query\" : \"prefix:que,prefix:pa\" }"
        doc = ArangoDB.log_put("#{prefix}-within", cmd, :body => body)

        doc.code.should eq(201)
        doc.headers['content-type'].should eq("application/json; charset=utf-8")
        doc.parsed_response['error'].should eq(false)
        doc.parsed_response['code'].should eq(201)
        doc.parsed_response['hasMore'].should eq(false)
        doc.parsed_response['result'].length.should eq(3)
        doc.parsed_response['count'].should eq(3)
      end
    end

  end
end
