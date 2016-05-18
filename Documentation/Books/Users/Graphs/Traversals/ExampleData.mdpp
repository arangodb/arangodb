!CHAPTER Example Data

The following examples all use a vertex collection *v* and an [edge collection](../../Appendix/Glossary.md#edge-collection) *e*. The vertex
collection *v* contains continents, countries, and capitals. The edge collection *e* 
contains connections between continents and countries, and between countries and capitals.

To set up the collections and populate them with initial data, the following script was used:

```js
db._create("v");
db._createEdgeCollection("e");

// vertices: root node 
db.v.save({ _key: "world", name: "World", type: "root" });

// vertices: continents 
db.v.save({ _key: "continent-africa", name: "Africa", type: "continent" });
db.v.save({ _key: "continent-asia", name: "Asia", type: "continent" });
db.v.save({ _key: "continent-australia", name: "Australia", type: "continent" });
db.v.save({ _key: "continent-europe", name: "Europe", type: "continent" });
db.v.save({ _key: "continent-north-america", name: "North America", type: "continent" });
db.v.save({ _key: "continent-south-america", name: "South America", type: "continent" });

// vertices: countries 
db.v.save({ _key: "country-afghanistan", name: "Afghanistan", type: "country", code: "AFG" });
db.v.save({ _key: "country-albania", name: "Albania", type: "country", code: "ALB" });
db.v.save({ _key: "country-algeria", name: "Algeria", type: "country", code: "DZA" });
db.v.save({ _key: "country-andorra", name: "Andorra", type: "country", code: "AND" });
db.v.save({ _key: "country-angola", name: "Angola", type: "country", code: "AGO" });
db.v.save({ _key: "country-antigua-and-barbuda", name: "Antigua and Barbuda", type: "country", code: "ATG" });
db.v.save({ _key: "country-argentina", name: "Argentina", type: "country", code: "ARG" });
db.v.save({ _key: "country-australia", name: "Australia", type: "country", code: "AUS" });
db.v.save({ _key: "country-austria", name: "Austria", type: "country", code: "AUT" });
db.v.save({ _key: "country-bahamas", name: "Bahamas", type: "country", code: "BHS" });
db.v.save({ _key: "country-bahrain", name: "Bahrain", type: "country", code: "BHR" });
db.v.save({ _key: "country-bangladesh", name: "Bangladesh", type: "country", code: "BGD" });
db.v.save({ _key: "country-barbados", name: "Barbados", type: "country", code: "BRB" });
db.v.save({ _key: "country-belgium", name: "Belgium", type: "country", code: "BEL" });
db.v.save({ _key: "country-bhutan", name: "Bhutan", type: "country", code: "BTN" });
db.v.save({ _key: "country-bolivia", name: "Bolivia", type: "country", code: "BOL" });
db.v.save({ _key: "country-bosnia-and-herzegovina", name: "Bosnia and Herzegovina", type: "country", code: "BIH" });
db.v.save({ _key: "country-botswana", name: "Botswana", type: "country", code: "BWA" });
db.v.save({ _key: "country-brazil", name: "Brazil", type: "country", code: "BRA" });
db.v.save({ _key: "country-brunei", name: "Brunei", type: "country", code: "BRN" });
db.v.save({ _key: "country-bulgaria", name: "Bulgaria", type: "country", code: "BGR" });
db.v.save({ _key: "country-burkina-faso", name: "Burkina Faso", type: "country", code: "BFA" });
db.v.save({ _key: "country-burundi", name: "Burundi", type: "country", code: "BDI" });
db.v.save({ _key: "country-cambodia", name: "Cambodia", type: "country", code: "KHM" });
db.v.save({ _key: "country-cameroon", name: "Cameroon", type: "country", code: "CMR" });
db.v.save({ _key: "country-canada", name: "Canada", type: "country", code: "CAN" });
db.v.save({ _key: "country-chad", name: "Chad", type: "country", code: "TCD" });
db.v.save({ _key: "country-chile", name: "Chile", type: "country", code: "CHL" });
db.v.save({ _key: "country-colombia", name: "Colombia", type: "country", code: "COL" });
db.v.save({ _key: "country-cote-d-ivoire", name: "Cote d'Ivoire", type: "country", code: "CIV" });
db.v.save({ _key: "country-croatia", name: "Croatia", type: "country", code: "HRV" });
db.v.save({ _key: "country-czech-republic", name: "Czech Republic", type: "country", code: "CZE" });
db.v.save({ _key: "country-denmark", name: "Denmark", type: "country", code: "DNK" });
db.v.save({ _key: "country-ecuador", name: "Ecuador", type: "country", code: "ECU" });
db.v.save({ _key: "country-egypt", name: "Egypt", type: "country", code: "EGY" });
db.v.save({ _key: "country-eritrea", name: "Eritrea", type: "country", code: "ERI" });
db.v.save({ _key: "country-finland", name: "Finland", type: "country", code: "FIN" });
db.v.save({ _key: "country-france", name: "France", type: "country", code: "FRA" });
db.v.save({ _key: "country-germany", name: "Germany", type: "country", code: "DEU" });
db.v.save({ _key: "country-people-s-republic-of-china", name: "People's Republic of China", type: "country", code: "CHN" });

// vertices: capitals 
db.v.save({ _key: "capital-algiers", name: "Algiers", type: "capital" });
db.v.save({ _key: "capital-andorra-la-vella", name: "Andorra la Vella", type: "capital" });
db.v.save({ _key: "capital-asmara", name: "Asmara", type: "capital" });
db.v.save({ _key: "capital-bandar-seri-begawan", name: "Bandar Seri Begawan", type: "capital" });
db.v.save({ _key: "capital-beijing", name: "Beijing", type: "capital" });
db.v.save({ _key: "capital-berlin", name: "Berlin", type: "capital" });
db.v.save({ _key: "capital-bogota", name: "Bogota", type: "capital" });
db.v.save({ _key: "capital-brasilia", name: "Brasilia", type: "capital" });
db.v.save({ _key: "capital-bridgetown", name: "Bridgetown", type: "capital" });
db.v.save({ _key: "capital-brussels", name: "Brussels", type: "capital" });
db.v.save({ _key: "capital-buenos-aires", name: "Buenos Aires", type: "capital" });
db.v.save({ _key: "capital-bujumbura", name: "Bujumbura", type: "capital" });
db.v.save({ _key: "capital-cairo", name: "Cairo", type: "capital" });
db.v.save({ _key: "capital-canberra", name: "Canberra", type: "capital" });
db.v.save({ _key: "capital-copenhagen", name: "Copenhagen", type: "capital" });
db.v.save({ _key: "capital-dhaka", name: "Dhaka", type: "capital" });
db.v.save({ _key: "capital-gaborone", name: "Gaborone", type: "capital" });
db.v.save({ _key: "capital-helsinki", name: "Helsinki", type: "capital" });
db.v.save({ _key: "capital-kabul", name: "Kabul", type: "capital" });
db.v.save({ _key: "capital-la-paz", name: "La Paz", type: "capital" });
db.v.save({ _key: "capital-luanda", name: "Luanda", type: "capital" });
db.v.save({ _key: "capital-manama", name: "Manama", type: "capital" });
db.v.save({ _key: "capital-nassau", name: "Nassau", type: "capital" });
db.v.save({ _key: "capital-n-djamena", name: "N'Djamena", type: "capital" });
db.v.save({ _key: "capital-ottawa", name: "Ottawa", type: "capital" });
db.v.save({ _key: "capital-ouagadougou", name: "Ouagadougou", type: "capital" });
db.v.save({ _key: "capital-paris", name: "Paris", type: "capital" });
db.v.save({ _key: "capital-phnom-penh", name: "Phnom Penh", type: "capital" });
db.v.save({ _key: "capital-prague", name: "Prague", type: "capital" });
db.v.save({ _key: "capital-quito", name: "Quito", type: "capital" });
db.v.save({ _key: "capital-saint-john-s", name: "Saint John's", type: "capital" });
db.v.save({ _key: "capital-santiago", name: "Santiago", type: "capital" });
db.v.save({ _key: "capital-sarajevo", name: "Sarajevo", type: "capital" });
db.v.save({ _key: "capital-sofia", name: "Sofia", type: "capital" });
db.v.save({ _key: "capital-thimphu", name: "Thimphu", type: "capital" });
db.v.save({ _key: "capital-tirana", name: "Tirana", type: "capital" });
db.v.save({ _key: "capital-vienna", name: "Vienna", type: "capital" });
db.v.save({ _key: "capital-yamoussoukro", name: "Yamoussoukro", type: "capital" });
db.v.save({ _key: "capital-yaounde", name: "Yaounde", type: "capital" });
db.v.save({ _key: "capital-zagreb", name: "Zagreb", type: "capital" });

// edges: continent -> world 
db.e.save("v/continent-africa", "v/world", { type: "is-in" });
db.e.save("v/continent-asia", "v/world", { type: "is-in" });
db.e.save("v/continent-australia", "v/world", { type: "is-in" });
db.e.save("v/continent-europe", "v/world", { type: "is-in" });
db.e.save("v/continent-north-america", "v/world", { type: "is-in" });
db.e.save("v/continent-south-america", "v/world", { type: "is-in" });

// edges: country -> continent 
db.e.save("v/country-afghanistan", "v/continent-asia", { type: "is-in" });
db.e.save("v/country-albania", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-algeria", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-andorra", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-angola", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-antigua-and-barbuda", "v/continent-north-america", { type: "is-in" });
db.e.save("v/country-argentina", "v/continent-south-america", { type: "is-in" });
db.e.save("v/country-australia", "v/continent-australia", { type: "is-in" });
db.e.save("v/country-austria", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-bahamas", "v/continent-north-america", { type: "is-in" });
db.e.save("v/country-bahrain", "v/continent-asia", { type: "is-in" });
db.e.save("v/country-bangladesh", "v/continent-asia", { type: "is-in" });
db.e.save("v/country-barbados", "v/continent-north-america", { type: "is-in" });
db.e.save("v/country-belgium", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-bhutan", "v/continent-asia", { type: "is-in" });
db.e.save("v/country-bolivia", "v/continent-south-america", { type: "is-in" });
db.e.save("v/country-bosnia-and-herzegovina", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-botswana", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-brazil", "v/continent-south-america", { type: "is-in" });
db.e.save("v/country-brunei", "v/continent-asia", { type: "is-in" });
db.e.save("v/country-bulgaria", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-burkina-faso", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-burundi", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-cambodia", "v/continent-asia", { type: "is-in" });
db.e.save("v/country-cameroon", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-canada", "v/continent-north-america", { type: "is-in" });
db.e.save("v/country-chad", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-chile", "v/continent-south-america", { type: "is-in" });
db.e.save("v/country-colombia", "v/continent-south-america", { type: "is-in" });
db.e.save("v/country-cote-d-ivoire", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-croatia", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-czech-republic", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-denmark", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-ecuador", "v/continent-south-america", { type: "is-in" });
db.e.save("v/country-egypt", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-eritrea", "v/continent-africa", { type: "is-in" });
db.e.save("v/country-finland", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-france", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-germany", "v/continent-europe", { type: "is-in" });
db.e.save("v/country-people-s-republic-of-china", "v/continent-asia", { type: "is-in" });

// edges: capital -> country 
db.e.save("v/capital-algiers", "v/country-algeria", { type: "is-in" });
db.e.save("v/capital-andorra-la-vella", "v/country-andorra", { type: "is-in" });
db.e.save("v/capital-asmara", "v/country-eritrea", { type: "is-in" });
db.e.save("v/capital-bandar-seri-begawan", "v/country-brunei", { type: "is-in" });
db.e.save("v/capital-beijing", "v/country-people-s-republic-of-china", { type: "is-in" });
db.e.save("v/capital-berlin", "v/country-germany", { type: "is-in" });
db.e.save("v/capital-bogota", "v/country-colombia", { type: "is-in" });
db.e.save("v/capital-brasilia", "v/country-brazil", { type: "is-in" });
db.e.save("v/capital-bridgetown", "v/country-barbados", { type: "is-in" });
db.e.save("v/capital-brussels", "v/country-belgium", { type: "is-in" });
db.e.save("v/capital-buenos-aires", "v/country-argentina", { type: "is-in" });
db.e.save("v/capital-bujumbura", "v/country-burundi", { type: "is-in" });
db.e.save("v/capital-cairo", "v/country-egypt", { type: "is-in" });
db.e.save("v/capital-canberra", "v/country-australia", { type: "is-in" });
db.e.save("v/capital-copenhagen", "v/country-denmark", { type: "is-in" });
db.e.save("v/capital-dhaka", "v/country-bangladesh", { type: "is-in" });
db.e.save("v/capital-gaborone", "v/country-botswana", { type: "is-in" });
db.e.save("v/capital-helsinki", "v/country-finland", { type: "is-in" });
db.e.save("v/capital-kabul", "v/country-afghanistan", { type: "is-in" });
db.e.save("v/capital-la-paz", "v/country-bolivia", { type: "is-in" });
db.e.save("v/capital-luanda", "v/country-angola", { type: "is-in" });
db.e.save("v/capital-manama", "v/country-bahrain", { type: "is-in" });
db.e.save("v/capital-nassau", "v/country-bahamas", { type: "is-in" });
db.e.save("v/capital-n-djamena", "v/country-chad", { type: "is-in" });
db.e.save("v/capital-ottawa", "v/country-canada", { type: "is-in" });
db.e.save("v/capital-ouagadougou", "v/country-burkina-faso", { type: "is-in" });
db.e.save("v/capital-paris", "v/country-france", { type: "is-in" });
db.e.save("v/capital-phnom-penh", "v/country-cambodia", { type: "is-in" });
db.e.save("v/capital-prague", "v/country-czech-republic", { type: "is-in" });
db.e.save("v/capital-quito", "v/country-ecuador", { type: "is-in" });
db.e.save("v/capital-saint-john-s", "v/country-antigua-and-barbuda", { type: "is-in" });
db.e.save("v/capital-santiago", "v/country-chile", { type: "is-in" });
db.e.save("v/capital-sarajevo", "v/country-bosnia-and-herzegovina", { type: "is-in" });
db.e.save("v/capital-sofia", "v/country-bulgaria", { type: "is-in" });
db.e.save("v/capital-thimphu", "v/country-bhutan", { type: "is-in" });
db.e.save("v/capital-tirana", "v/country-albania", { type: "is-in" });
db.e.save("v/capital-vienna", "v/country-austria", { type: "is-in" });
db.e.save("v/capital-yamoussoukro", "v/country-cote-d-ivoire", { type: "is-in" });
db.e.save("v/capital-yaounde", "v/country-cameroon", { type: "is-in" });
db.e.save("v/capital-zagreb", "v/country-croatia", { type: "is-in" });
```
