'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief example-users
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;

// //////////////////////////////////////////////////////////////////////////////
// / @brief user data
// //////////////////////////////////////////////////////////////////////////////

var data = [
  {'name': {'first': 'Lue','last': 'Laserna'},
    'gender': 'female',
    'birthday': '1983-09-18',
    'contact': {'address': {'street': '19 Deer Loop','zip': '90732','city': 'San Pedro','state': 'CA'},
      'email': ['lue.laserna@nosql-matters.org', 'laserna@nosql-matters.org'],
    'region': '310','phone': ['310-8268551', '310-7618427']},
    'likes': ['chatting'],
  'memberSince': '2011-05-05'},

  {'name': {'first': 'Jasper','last': 'Grebel'},
    'gender': 'male',
    'birthday': '1989-04-29',
    'contact': {'address': {'street': '19 Florida Loop','zip': '66840','city': 'Burns','state': 'KS'},
      'email': ['jasper.grebel@nosql-matters.org'],
    'region': '316','phone': ['316-2417120', '316-2767391']},
    'likes': ['shopping'],
  'memberSince': '2011-11-10'},

  {'name': {'first': 'Kandra','last': 'Beichner'},
    'gender': 'female',
    'birthday': '1963-11-21',
    'contact': {'address': {'street': '6 John Run','zip': '98434','city': 'Tacoma','state': 'WA'},
      'email': ['kandra.beichner@nosql-matters.org', 'kandra@nosql-matters.org'],
    'region': '253','phone': ['253-0405964']},
    'likes': ['swimming'],
  'memberSince': '2012-03-18'},

  {'name': {'first': 'Jeff','last': 'Schmith'},
    'gender': 'male',
    'birthday': '1977-10-14',
    'contact': {'address': {'street': '14 198th St','zip': '72569','city': 'Poughkeepsie','state': 'AR'},
      'email': ['jeff.schmith@nosql-matters.org'],
    'region': '870','phone': []},
    'likes': ['chatting', 'boxing', 'reading'],
  'memberSince': '2011-02-10'},

  {'name': {'first': 'Tuan','last': 'Climer'},
    'gender': 'male',
    'birthday': '1951-04-06',
    'contact': {'address': {'street': '6 Kansas St','zip': '07921','city': 'Bedminster','state': 'NJ'},
      'email': ['tuan.climer@nosql-matters.org'],
    'region': '908','phone': ['908-8376478']},
    'likes': ['ironing'],
  'memberSince': '2011-02-06'},

  {'name': {'first': 'Warner','last': 'Lemaire'},
    'gender': 'male',
    'birthday': '1953-05-01',
    'contact': {'address': {'street': '14 234th St','zip': '22553','city': 'Spotsylvania','state': 'VA'},
      'email': ['warner.lemaire@nosql-matters.org', 'warner@nosql-matters.org'],
    'region': '540','phone': []},
    'likes': ['driving'],
  'memberSince': '2008-10-20'},

  {'name': {'first': 'Hugh','last': 'Potash'},
    'gender': 'male',
    'birthday': '1971-02-17',
    'contact': {'address': {'street': '16 Beechwood Way','zip': '14902','city': 'Elmira','state': 'NY'},
      'email': ['hugh.potash@nosql-matters.org', 'potash@nosql-matters.org', 'hugh@nosql-matters.org'],
    'region': '607','phone': ['607-5183546']},
    'likes': [],
  'memberSince': '2011-10-28'},

  {'name': {'first': 'Jennefer','last': 'Menning'},
    'gender': 'female',
    'birthday': '1972-09-11',
    'contact': {'address': {'street': '1 Euclid Dr','zip': '56307','city': 'Albany','state': 'MN'},
      'email': ['jennefer.menning@nosql-matters.org', 'jennefer@nosql-matters.org'],
    'region': '320','phone': []},
    'likes': [],
  'memberSince': '2010-09-11'},

  {'name': {'first': 'Claude','last': 'Willcott'},
    'gender': 'female',
    'birthday': '1979-01-08',
    'contact': {'address': {'street': '2 Country club Ave','zip': '50588','city': 'Storm Lake','state': 'IA'},
      'email': ['claude.willcott@nosql-matters.org', 'willcott@nosql-matters.org', 'claude@nosql-matters.org'],
    'region': '712','phone': ['712-3896363']},
    'likes': ['chess', 'driving'],
  'memberSince': '2010-03-27'},

  {'name': {'first': 'Maximina','last': 'Kilzer'},
    'gender': 'female',
    'birthday': '1992-03-10',
    'contact': {'address': {'street': '4 Phillips Ln','zip': '73726','city': 'Carmen','state': 'OK'},
      'email': ['maximina.kilzer@nosql-matters.org', 'kilzer@nosql-matters.org'],
    'region': '580','phone': ['580-7678062']},
    'likes': ['shopping', 'travelling'],
  'memberSince': '2007-09-12'},

  {'name': {'first': 'Calvin','last': 'Porro'},
    'gender': 'male',
    'birthday': '1940-03-31',
    'contact': {'address': {'street': '20 12th Ave','zip': '59820','city': 'Alberton','state': 'MT'},
      'email': ['calvin.porro@nosql-matters.org'],
    'region': '406','phone': ['406-7464035']},
    'likes': [],
  'memberSince': '2009-10-17'},

  {'name': {'first': 'Diedre','last': 'Clinton'},
    'gender': 'female',
    'birthday': '1959-11-06',
    'contact': {'address': {'street': '2 Fraser Ave','zip': '81223','city': 'Cotopaxi','state': 'CO'},
      'email': ['diedre.clinton@nosql-matters.org', 'clinton@nosql-matters.org', 'diedre@nosql-matters.org'],
    'region': '719','phone': ['719-7055896']},
    'likes': ['swimming'],
  'memberSince': '2009-03-14'},

  {'name': {'first': 'Lavone','last': 'Peery'},
    'gender': 'female',
    'birthday': '1960-07-23',
    'contact': {'address': {'street': '7 Wyoming Hwy','zip': '83612','city': 'Council','state': 'ID'},
      'email': ['lavone.peery@nosql-matters.org', 'lavone@nosql-matters.org'],
    'region': '208','phone': ['208-6845728', '208-9763317']},
    'likes': [],
  'memberSince': '2010-07-17'},

  {'name': {'first': 'Stephen','last': 'Jakovac'},
    'gender': 'male',
    'birthday': '1985-08-11',
    'contact': {'address': {'street': '10 4th St','zip': '56308','city': 'Alexandria','state': 'MN'},
      'email': ['stephen.jakovac@nosql-matters.org'],
    'region': '320','phone': ['320-2503176', '320-9515697']},
    'likes': [],
  'memberSince': '2010-07-21'},

  {'name': {'first': 'Cleveland','last': 'Bejcek'},
    'gender': 'male',
    'birthday': '1944-07-30',
    'contact': {'address': {'street': '14 Patriot Hwy','zip': '95062','city': 'Santa Cruz','state': 'CA'},
      'email': ['cleveland.bejcek@nosql-matters.org', 'cleveland@nosql-matters.org'],
    'region': '831','phone': []},
    'likes': ['chess', 'checkers'],
  'memberSince': '2011-12-26'},

  {'name': {'first': 'Mickie','last': 'Menchaca'},
    'gender': 'female',
    'birthday': '1960-02-08',
    'contact': {'address': {'street': '1 Kansas Aly','zip': '08722','city': 'Beachwood','state': 'NJ'},
      'email': ['mickie.menchaca@nosql-matters.org', 'mickie@nosql-matters.org'],
    'region': '732','phone': ['732-1143581']},
    'likes': [],
  'memberSince': '2011-05-06'},

  {'name': {'first': 'Jason','last': 'Meneley'},
    'gender': 'male',
    'birthday': '1971-02-24',
    'contact': {'address': {'street': '9 Spring garden Dr','zip': '13618','city': 'Cape Vincent','state': 'NY'},
      'email': ['jason.meneley@nosql-matters.org', 'jason@nosql-matters.org'],
    'region': '315','phone': ['315-7142142', '315-0405535']},
    'likes': ['climbing'],
  'memberSince': '2009-08-15'},

  {'name': {'first': 'Fredda','last': 'Persten'},
    'gender': 'female',
    'birthday': '1944-07-25',
    'contact': {'address': {'street': '6 University Cir','zip': '88546','city': 'El Paso','state': 'TX'},
      'email': ['fredda.persten@nosql-matters.org', 'persten@nosql-matters.org'],
    'region': '915','phone': ['915-7112133', '915-5032376']},
    'likes': [],
  'memberSince': '2009-11-12'},

  {'name': {'first': 'Ann','last': 'Skaar'},
    'gender': 'female',
    'birthday': '1971-10-03',
    'contact': {'address': {'street': '6 17th Ave','zip': '59212','city': 'Bainville','state': 'MT'},
      'email': ['ann.skaar@nosql-matters.org', 'skaar@nosql-matters.org'],
    'region': '406','phone': []},
    'likes': ['boxing'],
  'memberSince': '2009-10-11'},

  {'name': {'first': 'Corey','last': 'Shiroma'},
    'gender': 'male',
    'birthday': '1989-06-08',
    'contact': {'address': {'street': '19 Crescent Loop','zip': '63108','city': 'Saint Louis','state': 'MO'},
      'email': ['corey.shiroma@nosql-matters.org', 'shiroma@nosql-matters.org'],
    'region': '314','phone': []},
    'likes': ['swimming', 'driving'],
  'memberSince': '2009-02-05'},

  {'name': {'first': 'Graig','last': 'Flax'},
    'gender': 'male',
    'birthday': '1944-10-03',
    'contact': {'address': {'street': '7 Wallick Ln','zip': '05442','city': 'Belvidere Center','state': 'VT'},
      'email': ['graig.flax@nosql-matters.org', 'graig@nosql-matters.org'],
    'region': '802','phone': ['802-4827967']},
    'likes': ['skiing', 'climbing'],
  'memberSince': '2011-07-01'},

  {'name': {'first': 'Nydia','last': 'Weeden'},
    'gender': 'female',
    'birthday': '1962-05-30',
    'contact': {'address': {'street': '6 Mozart St','zip': '33612','city': 'Tampa','state': 'FL'},
      'email': ['nydia.weeden@nosql-matters.org'],
    'region': '813','phone': ['813-5324600', '813-4712316']},
    'likes': [],
  'memberSince': '2007-10-12'},

  {'name': {'first': 'Minerva','last': 'Reinbold'},
    'gender': 'female',
    'birthday': '1957-12-24',
    'contact': {'address': {'street': '7 Branchton Ln','zip': '88021','city': 'Anthony','state': 'NM'},
      'email': ['minerva.reinbold@nosql-matters.org'],
    'region': '505','phone': []},
    'likes': ['chess', 'climbing'],
  'memberSince': '2007-06-10'},

  {'name': {'first': 'Lou','last': 'Cheroki'},
    'gender': 'male',
    'birthday': '1975-06-27',
    'contact': {'address': {'street': '10 Connecticut Dr','zip': '71465','city': 'Olla','state': 'LA'},
      'email': ['lou.cheroki@nosql-matters.org', 'cheroki@nosql-matters.org'],
    'region': '318','phone': ['318-5241811']},
    'likes': [],
  'memberSince': '2009-01-22'},

  {'name': {'first': 'Frank','last': 'Sedano'},
    'gender': 'male',
    'birthday': '1988-09-10',
    'contact': {'address': {'street': '17 Woodson Ave','zip': '66773','city': 'Scammon','state': 'KS'},
      'email': ['frank.sedano@nosql-matters.org', 'sedano@nosql-matters.org'],
    'region': '316','phone': []},
    'likes': ['ironing'],
  'memberSince': '2009-10-22'},

  {'name': {'first': 'Tracey','last': 'Braylock'},
    'gender': 'male',
    'birthday': '1941-10-17',
    'contact': {'address': {'street': '10 Wallick Run','zip': '61442','city': 'Keithsburg','state': 'IL'},
      'email': ['tracey.braylock@nosql-matters.org', 'braylock@nosql-matters.org'],
    'region': '309','phone': []},
    'likes': [],
  'memberSince': '2010-12-21'},

  {'name': {'first': 'Hayden','last': 'Daniel'},
    'gender': 'male',
    'birthday': '1990-06-15',
    'contact': {'address': {'street': '4 Main Loop','zip': '66411','city': 'Blue Rapids','state': 'KS'},
      'email': ['hayden.daniel@nosql-matters.org', 'daniel@nosql-matters.org', 'hayden@nosql-matters.org'],
    'region': '785','phone': ['785-4595093', '785-9794490']},
    'likes': ['travelling'],
  'memberSince': '2010-05-11'},

  {'name': {'first': 'Jene','last': 'Sance'},
    'gender': 'female',
    'birthday': '1960-01-10',
    'contact': {'address': {'street': '12 Mound Ln','zip': '37656','city': 'Fall Branch','state': 'TN'},
      'email': ['jene.sance@nosql-matters.org', 'sance@nosql-matters.org', 'jene@nosql-matters.org'],
    'region': '423','phone': []},
    'likes': [],
  'memberSince': '2008-02-28'},

  {'name': {'first': 'Toccara','last': 'Damato'},
    'gender': 'female',
    'birthday': '1983-02-21',
    'contact': {'address': {'street': '6 Deer Way','zip': '59445','city': 'Garneill','state': 'MT'},
      'email': ['toccara.damato@nosql-matters.org', 'toccara@nosql-matters.org'],
    'region': '406','phone': ['406-4971630']},
    'likes': ['running', 'snowboarding'],
  'memberSince': '2008-07-19'},

  {'name': {'first': 'Josefina','last': 'Hams'},
    'gender': 'female',
    'birthday': '1984-07-29',
    'contact': {'address': {'street': '7 Center St','zip': '19021','city': 'Croydon','state': 'PA'},
      'email': ['josefina.hams@nosql-matters.org'],
    'region': '215','phone': ['215-9059514', '215-9507320']},
    'likes': ['chess', 'boxing'],
  'memberSince': '2007-09-28'},

  {'name': {'first': 'Randy','last': 'Klenovich'},
    'gender': 'male',
    'birthday': '1984-01-28',
    'contact': {'address': {'street': '22 3rd Ave','zip': '28524','city': 'Davis','state': 'NC'},
      'email': ['randy.klenovich@nosql-matters.org'],
    'region': '252','phone': ['252-4611502', '252-6980326']},
    'likes': ['chess', 'boxing'],
  'memberSince': '2010-03-11'},

  {'name': {'first': 'Gonzalo','last': 'Boeshore'},
    'gender': 'male',
    'birthday': '1942-10-07',
    'contact': {'address': {'street': '18 Willard Dr','zip': '48310','city': 'Sterling Heights','state': 'MI'},
      'email': ['gonzalo.boeshore@nosql-matters.org'],
    'region': '810','phone': ['810-6542772']},
    'likes': [],
  'memberSince': '2009-10-26'},

  {'name': {'first': 'Hai','last': 'Treamer'},
    'gender': 'male',
    'birthday': '1944-04-09',
    'contact': {'address': {'street': '6 Deer St','zip': '51108','city': 'Sioux City','state': 'IA'},
      'email': ['hai.treamer@nosql-matters.org'],
    'region': '712','phone': ['712-5347242', '712-7460448']},
    'likes': [],
  'memberSince': '2010-09-30'},

  {'name': {'first': 'Sammy','last': 'Coldivar'},
    'gender': 'female',
    'birthday': '1982-04-07',
    'contact': {'address': {'street': '2 Commercial Way','zip': '88025','city': 'Buckhorn','state': 'NM'},
      'email': ['sammy.coldivar@nosql-matters.org'],
    'region': '505','phone': ['505-1394678']},
    'likes': [],
  'memberSince': '2012-04-30'},

  {'name': {'first': 'Son','last': 'Dabe'},
    'gender': 'female',
    'birthday': '1943-11-20',
    'contact': {'address': {'street': '20 Beechwood Ln','zip': '66522','city': 'Oneida','state': 'KS'},
      'email': ['son.dabe@nosql-matters.org'],
    'region': '785','phone': ['785-4240372']},
    'likes': ['checkers'],
  'memberSince': '2011-05-25'},

  {'name': {'first': 'Carlee','last': 'Degenfelder'},
    'gender': 'female',
    'birthday': '1981-06-24',
    'contact': {'address': {'street': '22 Laramie Cir','zip': '91189','city': 'Pasadena','state': 'CA'},
      'email': ['carlee.degenfelder@nosql-matters.org', 'degenfelder@nosql-matters.org',
        'carlee@nosql-matters.org'],
    'region': '626','phone': ['626-4051264', '626-2789580']},
    'likes': [],
  'memberSince': '2010-09-21'},

  {'name': {'first': 'Mose','last': 'Backlund'},
    'gender': 'male',
    'birthday': '1951-08-03',
    'contact': {'address': {'street': '1 1st Ave','zip': '30751','city': 'Tennga','state': 'GA'},
      'email': ['mose.backlund@nosql-matters.org', 'backlund@nosql-matters.org'],
    'region': '706','phone': []},
    'likes': ['biking', 'travelling'],
  'memberSince': '2011-10-25'},

  {'name': {'first': 'Clayton','last': 'Hilda'},
    'gender': 'male',
    'birthday': '1940-01-03',
    'contact': {'address': {'street': '22 18th St','zip': '14715','city': 'Bolivar','state': 'NY'},
      'email': ['clayton.hilda@nosql-matters.org', 'clayton@nosql-matters.org'],
    'region': '716','phone': ['716-0043859', '716-7107175']},
    'likes': [],
  'memberSince': '2011-02-09'},

  {'name': {'first': 'Charley','last': 'Demora'},
    'gender': 'male',
    'birthday': '1955-08-06',
    'contact': {'address': {'street': '5 6th St','zip': '01536','city': 'North Grafton','state': 'MA'},
      'email': ['charley.demora@nosql-matters.org'],
    'region': '508','phone': ['508-9084206', '508-1037607']},
    'likes': [],
  'memberSince': '2007-07-24'},

  {'name': {'first': 'Dorene','last': 'Gunther'},
    'gender': 'female',
    'birthday': '1955-01-26',
    'contact': {'address': {'street': '4 Main Run','zip': '01910','city': 'Lynn','state': 'MA'},
      'email': ['dorene.gunther@nosql-matters.org', 'gunther@nosql-matters.org'],
    'region': '781','phone': ['781-5720472']},
    'likes': ['chatting', 'boxing', 'driving', 'ironing'],
  'memberSince': '2010-01-28'},

  {'name': {'first': 'Alane','last': 'Goldade'},
    'gender': 'female',
    'birthday': '1991-11-20',
    'contact': {'address': {'street': '22 Wyoming Ct','zip': '97206','city': 'Portland','state': 'OR'},
      'email': ['alane.goldade@nosql-matters.org', 'alane@nosql-matters.org'],
    'region': '503','phone': ['503-3482925', '503-9205542']},
    'likes': [],
  'memberSince': '2011-02-09'},

  {'name': {'first': 'Salina','last': 'Sue'},
    'gender': 'female',
    'birthday': '1962-11-08',
    'contact': {'address': {'street': '4 New hampshire Way','zip': '61759','city': 'Minier','state': 'IL'},
      'email': ['salina.sue@nosql-matters.org'],
    'region': '309','phone': ['309-7368050']},
    'likes': ['running', 'shopping'],
  'memberSince': '2012-05-02'},

  {'name': {'first': 'Rico','last': 'Hoopengardner'},
    'gender': 'male',
    'birthday': '1961-12-22',
    'contact': {'address': {'street': '14 Phillips Hwy','zip': '90242','city': 'Downey','state': 'CA'},
      'email': ['rico.hoopengardner@nosql-matters.org'],
    'region': '562','phone': ['562-1439117']},
    'likes': ['biking'],
  'memberSince': '2007-07-16'},

  {'name': {'first': 'Laurence','last': 'Vahena'},
    'gender': 'female',
    'birthday': '1987-03-25',
    'contact': {'address': {'street': '18 Santa fe Hwy','zip': '78055','city': 'Medina','state': 'TX'},
      'email': ['laurence.vahena@nosql-matters.org', 'laurence@nosql-matters.org'],
    'region': '830','phone': ['830-1060061']},
    'likes': ['running'],
  'memberSince': '2009-02-27'},

  {'name': {'first': 'Jasper','last': 'Okorududu'},
    'gender': 'male',
    'birthday': '1955-02-12',
    'contact': {'address': {'street': '7 21st St','zip': '72776','city': 'Witter','state': 'AR'},
      'email': ['jasper.okorududu@nosql-matters.org', 'okorududu@nosql-matters.org'],
    'region': '501','phone': ['501-7977106', '501-7138486']},
    'likes': ['boxing', 'climbing'],
  'memberSince': '2011-02-12'},

  {'name': {'first': 'Tyesha','last': 'Loehrer'},
    'gender': 'female',
    'birthday': '1959-01-30',
    'contact': {'address': {'street': '9 266th Ave','zip': '63023','city': 'Dittmer','state': 'MO'},
      'email': ['tyesha.loehrer@nosql-matters.org', 'tyesha@nosql-matters.org'],
    'region': '636','phone': []},
    'likes': ['boxing'],
  'memberSince': '2010-12-19'},

  {'name': {'first': 'Murray','last': 'Zirk'},
    'gender': 'male',
    'birthday': '1946-07-14',
    'contact': {'address': {'street': '10 Main Ct','zip': '00936','city': 'San Juan','state': 'PR'},
      'email': ['murray.zirk@nosql-matters.org', 'zirk@nosql-matters.org'],
    'region': '787','phone': ['787-5001534', '787-8378996']},
    'likes': [],
  'memberSince': '2009-06-02'},

  {'name': {'first': 'Virgil','last': 'Mulneix'},
    'gender': 'female',
    'birthday': '1978-03-29',
    'contact': {'address': {'street': '12 Woodlawn St','zip': '35243','city': 'Birmingham','state': 'AL'},
      'email': ['virgil.mulneix@nosql-matters.org'],
    'region': '205','phone': ['205-9970849']},
    'likes': ['chatting', 'boxing'],
  'memberSince': '2010-07-09'},

  {'name': {'first': 'Miles','last': 'Norden'},
    'gender': 'male',
    'birthday': '1988-01-14',
    'contact': {'address': {'street': '19 Patriot Blvd','zip': '12490','city': 'West Camp','state': 'NY'},
      'email': ['miles.norden@nosql-matters.org'],
    'region': '914','phone': ['914-3581265', '914-5948065']},
    'likes': [],
  'memberSince': '2011-03-15'},

  {'name': {'first': 'Quinn','last': 'Cote'},
    'gender': 'female',
    'birthday': '1969-09-01',
    'contact': {'address': {'street': '10 Ottawa Run','zip': '24055','city': 'Bassett','state': 'VA'},
      'email': ['quinn.cote@nosql-matters.org', 'cote@nosql-matters.org'],
    'region': '540','phone': ['540-4505047', '540-3769551']},
    'likes': ['chatting'],
  'memberSince': '2012-01-04'},

  {'name': {'first': 'Shavonne','last': 'Finchum'},
    'gender': 'female',
    'birthday': '1962-02-12',
    'contact': {'address': {'street': '12 Vermont St','zip': '12577','city': 'Salisbury Mills','state': 'NY'},
      'email': ['shavonne.finchum@nosql-matters.org', 'finchum@nosql-matters.org'],
    'region': '914','phone': ['914-8440818']},
    'likes': [],
  'memberSince': '2011-01-07'},

  {'name': {'first': 'Justine','last': 'Girone'},
    'gender': 'female',
    'birthday': '1993-11-22',
    'contact': {'address': {'street': '13 Florida Hwy','zip': '64441','city': 'Denver','state': 'MO'},
      'email': ['justine.girone@nosql-matters.org', 'girone@nosql-matters.org'],
    'region': '660','phone': []},
    'likes': [],
  'memberSince': '2010-10-10'},

  {'name': {'first': 'Dorthea','last': 'Visnic'},
    'gender': 'female',
    'birthday': '1948-11-06',
    'contact': {'address': {'street': '14 New hampshire Way','zip': '92402','city': 'San Bernardino','state': 'CA'},
      'email': ['dorthea.visnic@nosql-matters.org', 'dorthea@nosql-matters.org'],
    'region': '909','phone': ['909-1143770']},
    'likes': [],
  'memberSince': '2010-03-27'},

  {'name': {'first': 'Rolland','last': 'Rodrigres'},
    'gender': 'male',
    'birthday': '1965-06-07',
    'contact': {'address': {'street': '17 16th Ave','zip': '06754','city': 'Cornwall Bridge','state': 'CT'},
      'email': ['rolland.rodrigres@nosql-matters.org'],
    'region': '860','phone': ['860-8120488']},
    'likes': [],
  'memberSince': '2009-03-27'},

  {'name': {'first': 'Elias','last': 'Morch'},
    'gender': 'male',
    'birthday': '1955-08-02',
    'contact': {'address': {'street': '13 Deer Ct','zip': '77248','city': 'Houston','state': 'TX'},
      'email': ['elias.morch@nosql-matters.org', 'morch@nosql-matters.org'],
    'region': '713','phone': ['713-2682695']},
    'likes': [],
  'memberSince': '2011-07-26'},

  {'name': {'first': 'Ashley','last': 'Byczek'},
    'gender': 'male',
    'birthday': '1981-03-13',
    'contact': {'address': {'street': '18 262nd Ave','zip': '07717','city': 'Avon by the Sea','state': 'NJ'},
      'email': ['ashley.byczek@nosql-matters.org', 'ashley@nosql-matters.org'],
    'region': '732','phone': ['732-7878019', '732-1294501']},
    'likes': ['chess', 'boxing', 'reading', 'snowboarding', 'ironing'],
  'memberSince': '2010-10-30'},

  {'name': {'first': 'Rosella','last': 'Anastos'},
    'gender': 'female',
    'birthday': '1971-09-04',
    'contact': {'address': {'street': '8 Main St','zip': '75497','city': 'Yantis','state': 'TX'},
      'email': ['rosella.anastos@nosql-matters.org', 'anastos@nosql-matters.org'],
    'region': '903','phone': []},
    'likes': ['chatting'],
  'memberSince': '2008-06-27'},

  {'name': {'first': 'Nicky','last': 'Hopkins'},
    'gender': 'male',
    'birthday': '1943-08-03',
    'contact': {'address': {'street': '20 Liberty Loop','zip': '12741','city': 'Hankins','state': 'NY'},
      'email': ['nicky.hopkins@nosql-matters.org', 'hopkins@nosql-matters.org'],
    'region': '914','phone': []},
    'likes': ['shopping'],
  'memberSince': '2011-03-25'},

  {'name': {'first': 'Marcy','last': 'Bloem'},
    'gender': 'female',
    'birthday': '1974-05-26',
    'contact': {'address': {'street': '10 Last chance Pl','zip': '24839','city': 'Hanover','state': 'WV'},
      'email': ['marcy.bloem@nosql-matters.org', 'marcy@nosql-matters.org'],
    'region': '304','phone': []},
    'likes': [],
  'memberSince': '2011-03-11'},

  {'name': {'first': 'Long','last': 'Mulinix'},
    'gender': 'male',
    'birthday': '1968-04-01',
    'contact': {'address': {'street': '17 Swann Way','zip': '44805','city': 'Ashland','state': 'OH'},
      'email': ['long.mulinix@nosql-matters.org'],
    'region': '419','phone': []},
    'likes': ['travelling', 'climbing'],
  'memberSince': '2009-09-11'},

  {'name': {'first': 'Marlen','last': 'Heusner'},
    'gender': 'female',
    'birthday': '1979-04-24',
    'contact': {'address': {'street': '16 Industrial Dr','zip': '05047','city': 'Hartford','state': 'VT'},
      'email': ['marlen.heusner@nosql-matters.org', 'heusner@nosql-matters.org'],
    'region': '802','phone': ['802-6968411', '802-9326817']},
    'likes': ['chatting'],
  'memberSince': '2009-11-07'},

  {'name': {'first': 'Tonie','last': 'Mauro'},
    'gender': 'female',
    'birthday': '1992-05-09',
    'contact': {'address': {'street': '4 Biltmore Ct','zip': '63954','city': 'Neelyville','state': 'MO'},
      'email': ['tonie.mauro@nosql-matters.org'],
    'region': '573','phone': ['573-7582822']},
    'likes': ['snowboarding'],
  'memberSince': '2008-11-20'},

  {'name': {'first': 'Gerard','last': 'Shroyer'},
    'gender': 'male',
    'birthday': '1949-11-17',
    'contact': {'address': {'street': '14 18th St','zip': '74535','city': 'Clarita','state': 'OK'},
      'email': ['gerard.shroyer@nosql-matters.org', 'shroyer@nosql-matters.org'],
    'region': '580','phone': ['580-9002737', '580-2706148']},
    'likes': ['chatting'],
  'memberSince': '2008-08-15'},

  {'name': {'first': 'Rosina','last': 'Leinen'},
    'gender': 'female',
    'birthday': '1968-12-25',
    'contact': {'address': {'street': '3 Palmer Ln','zip': '92253','city': 'La Quinta','state': 'CA'},
      'email': ['rosina.leinen@nosql-matters.org', 'rosina@nosql-matters.org'],
    'region': '760','phone': ['760-2596242']},
    'likes': ['chatting', 'reading'],
  'memberSince': '2008-11-28'},

  {'name': {'first': 'Abe','last': 'Troiani'},
    'gender': 'male',
    'birthday': '1993-07-26',
    'contact': {'address': {'street': '5 Beekman St','zip': '61748','city': 'Hudson','state': 'IL'},
      'email': ['abe.troiani@nosql-matters.org', 'troiani@nosql-matters.org', 'abe@nosql-matters.org'],
    'region': '309','phone': ['309-4888909', '309-3260513']},
    'likes': ['shopping'],
  'memberSince': '2010-07-16'},

  {'name': {'first': 'Carri','last': 'Mickler'},
    'gender': 'female',
    'birthday': '1952-12-11',
    'contact': {'address': {'street': '13 290th St','zip': '89570','city': 'Reno','state': 'NV'},
      'email': ['carri.mickler@nosql-matters.org'],
    'region': '775','phone': ['775-3352309']},
    'likes': [],
  'memberSince': '2011-09-15'},

  {'name': {'first': 'Julissa','last': 'Filosa'},
    'gender': 'female',
    'birthday': '1956-03-13',
    'contact': {'address': {'street': '8 Beechwood Aly','zip': '50801','city': 'Creston','state': 'IA'},
      'email': ['julissa.filosa@nosql-matters.org'],
    'region': '515','phone': ['515-0629512', '515-7012469']},
    'likes': ['ironing'],
  'memberSince': '2012-02-25'},

  {'name': {'first': 'Dusty','last': 'Sistrunk'},
    'gender': 'female',
    'birthday': '1952-08-15',
    'contact': {'address': {'street': '18 Santa fe Cir','zip': '50239','city': 'Saint Anthony','state': 'IA'},
      'email': ['dusty.sistrunk@nosql-matters.org'],
    'region': '515','phone': ['515-0638915', '515-6682501']},
    'likes': ['travelling'],
  'memberSince': '2011-12-29'},

  {'name': {'first': 'Brandon','last': 'Ulibarri'},
    'gender': 'male',
    'birthday': '1951-06-25',
    'contact': {'address': {'street': '9 Beechwood Aly','zip': '21113','city': 'Odenton','state': 'MD'},
      'email': ['brandon.ulibarri@nosql-matters.org', 'brandon@nosql-matters.org'],
    'region': '410','phone': []},
    'likes': [],
  'memberSince': '2011-07-05'},

  {'name': {'first': 'Angelina','last': 'Mcadoo'},
    'gender': 'female',
    'birthday': '1989-04-05',
    'contact': {'address': {'street': '20 Ottawa Pl','zip': '47952','city': 'Kingman','state': 'IN'},
      'email': ['angelina.mcadoo@nosql-matters.org'],
    'region': '765','phone': ['765-1949397', '765-4847931']},
    'likes': [],
  'memberSince': '2010-08-07'},

  {'name': {'first': 'Julio','last': 'Schwanebeck'},
    'gender': 'female',
    'birthday': '1993-05-01',
    'contact': {'address': {'street': '9 Country club Aly','zip': '89012','city': 'Henderson','state': 'NV'},
      'email': ['julio.schwanebeck@nosql-matters.org', 'schwanebeck@nosql-matters.org'],
    'region': '702','phone': ['702-0808220']},
    'likes': ['checkers'],
  'memberSince': '2008-03-22'},

  {'name': {'first': 'Mirian','last': 'Guzzardo'},
    'gender': 'female',
    'birthday': '1993-02-25',
    'contact': {'address': {'street': '10 290th Ave','zip': '91383','city': 'Santa Clarita','state': 'CA'},
      'email': ['mirian.guzzardo@nosql-matters.org', 'mirian@nosql-matters.org'],
    'region': '661','phone': ['661-4619498']},
    'likes': ['swimming'],
  'memberSince': '2011-04-07'},

  {'name': {'first': 'Sharri','last': 'Pletsch'},
    'gender': 'female',
    'birthday': '1975-05-27',
    'contact': {'address': {'street': '13 Fraser Ln','zip': '94153','city': 'San Francisco','state': 'CA'},
      'email': ['sharri.pletsch@nosql-matters.org', 'sharri@nosql-matters.org'],
    'region': '415','phone': ['415-0144394']},
    'likes': [],
  'memberSince': '2012-04-02'},

  {'name': {'first': 'Brenton','last': 'Lynskey'},
    'gender': 'male',
    'birthday': '1961-03-28',
    'contact': {'address': {'street': '20 Kearney Way','zip': '30906','city': 'Augusta','state': 'GA'},
      'email': ['brenton.lynskey@nosql-matters.org', 'lynskey@nosql-matters.org', 'brenton@nosql-matters.org'],
    'region': '706','phone': ['706-9360969']},
    'likes': [],
  'memberSince': '2008-01-15'},

  {'name': {'first': 'Lavern','last': 'Hamlet'},
    'gender': 'male',
    'birthday': '1967-02-12',
    'contact': {'address': {'street': '12 Calvert Blvd','zip': '89428','city': 'Silver City','state': 'NV'},
      'email': ['lavern.hamlet@nosql-matters.org'],
    'region': '775','phone': ['775-6709905', '775-9088264']},
    'likes': ['running'],
  'memberSince': '2012-01-20'},

  {'name': {'first': 'Monte','last': 'Mihlfeld'},
    'gender': 'male',
    'birthday': '1941-09-22',
    'contact': {'address': {'street': '21 5th St','zip': '55587','city': 'Monticello','state': 'MN'},
      'email': ['monte.mihlfeld@nosql-matters.org'],
    'region': '612','phone': ['612-6706220']},
    'likes': ['swimming'],
  'memberSince': '2010-03-01'},

  {'name': {'first': 'Bradford','last': 'Imperial'},
    'gender': 'male',
    'birthday': '1942-08-01',
    'contact': {'address': {'street': '3 Belmont Ct','zip': '63198','city': 'Saint Louis','state': 'MO'},
      'email': ['bradford.imperial@nosql-matters.org', 'bradford@nosql-matters.org'],
    'region': '314','phone': ['314-9336935']},
    'likes': ['ironing'],
  'memberSince': '2008-03-26'},

  {'name': {'first': 'Eryn','last': 'Daquilante'},
    'gender': 'female',
    'birthday': '1975-11-28',
    'contact': {'address': {'street': '11 20th St','zip': '02481','city': 'Wellesley Hills','state': 'MA'},
      'email': ['eryn.daquilante@nosql-matters.org', 'daquilante@nosql-matters.org'],
    'region': '781','phone': ['781-6373282']},
    'likes': [],
  'memberSince': '2011-03-19'},

  {'name': {'first': 'Roslyn','last': 'Aerni'},
    'gender': 'female',
    'birthday': '1956-06-19',
    'contact': {'address': {'street': '16 Champlain St','zip': '04108','city': 'Peaks Island','state': 'ME'},
      'email': ['roslyn.aerni@nosql-matters.org'],
    'region': '207','phone': []},
    'likes': ['shopping', 'reading'],
  'memberSince': '2008-06-08'},

  {'name': {'first': 'Chas','last': 'Cefalu'},
    'gender': 'male',
    'birthday': '1983-05-07',
    'contact': {'address': {'street': '4 Ontario Rd','zip': '40409','city': 'Brodhead','state': 'KY'},
      'email': ['chas.cefalu@nosql-matters.org'],
    'region': '606','phone': ['606-5073368']},
    'likes': [],
  'memberSince': '2011-06-12'},

  {'name': {'first': 'Berneice','last': 'Migliori'},
    'gender': 'female',
    'birthday': '1944-12-31',
    'contact': {'address': {'street': '9 4th St','zip': '53576','city': 'Orfordville','state': 'WI'},
      'email': ['berneice.migliori@nosql-matters.org'],
    'region': '608','phone': []},
    'likes': ['swimming'],
  'memberSince': '2008-08-30'},

  {'name': {'first': 'Patricia','last': 'Shoji'},
    'gender': 'male',
    'birthday': '1970-04-13',
    'contact': {'address': {'street': '15 Kansas Dr','zip': '38962','city': 'Tippo','state': 'MS'},
      'email': ['patricia.shoji@nosql-matters.org'],
    'region': '662','phone': ['662-2743361', '662-7499550']},
    'likes': [],
  'memberSince': '2009-06-12'},

  {'name': {'first': 'Kenton','last': 'Bagent'},
    'gender': 'male',
    'birthday': '1944-10-20',
    'contact': {'address': {'street': '22 Wallace Ln','zip': '68120','city': 'Omaha','state': 'NE'},
      'email': ['kenton.bagent@nosql-matters.org', 'bagent@nosql-matters.org'],
    'region': '402','phone': []},
    'likes': ['running', 'snowboarding', 'skiing'],
  'memberSince': '2009-10-12'},

  {'name': {'first': 'Elyse','last': 'Leis'},
    'gender': 'female',
    'birthday': '1974-04-23',
    'contact': {'address': {'street': '18 Fuller Ln','zip': '47459','city': 'Solsberry','state': 'IN'},
      'email': ['elyse.leis@nosql-matters.org'],
    'region': '812','phone': []},
    'likes': ['running'],
  'memberSince': '2008-06-29'},

  {'name': {'first': 'Maryjo','last': 'Tariq'},
    'gender': 'female',
    'birthday': '1977-02-09',
    'contact': {'address': {'street': '6 Morningside Pl','zip': '20137','city': 'Broad Run','state': 'VA'},
      'email': ['maryjo.tariq@nosql-matters.org'],
    'region': '540','phone': ['540-7768397']},
    'likes': ['swimming', 'snowboarding', 'skiing', 'ironing'],
  'memberSince': '2010-11-07'},

  {'name': {'first': 'Margaretta','last': 'Ogwynn'},
    'gender': 'female',
    'birthday': '1965-07-04',
    'contact': {'address': {'street': '13 Palmer Pl','zip': '57438','city': 'Faulkton','state': 'SD'},
      'email': ['margaretta.ogwynn@nosql-matters.org', 'ogwynn@nosql-matters.org'],
    'region': '605','phone': ['605-3511227']},
    'likes': [],
  'memberSince': '2010-12-28'},

  {'name': {'first': 'Glory','last': 'Mollenkopf'},
    'gender': 'female',
    'birthday': '1963-10-04',
    'contact': {'address': {'street': '6 1st Ave','zip': '11386','city': 'Ridgewood','state': 'NY'},
      'email': ['glory.mollenkopf@nosql-matters.org'],
    'region': '718','phone': []},
    'likes': [],
  'memberSince': '2011-09-06'},

  {'name': {'first': 'Janette','last': 'Nicholes'},
    'gender': 'female',
    'birthday': '1988-07-09',
    'contact': {'address': {'street': '7 Rawlins Pl','zip': '10011','city': 'New York','state': 'NY'},
      'email': ['janette.nicholes@nosql-matters.org'],
    'region': '212','phone': ['212-6481067']},
    'likes': ['checkers'],
  'memberSince': '2011-05-27'},

  {'name': {'first': 'Brandon','last': 'Laidlaw'},
    'gender': 'male',
    'birthday': '1948-01-07',
    'contact': {'address': {'street': '21 Cliffbourne Aly','zip': '35240','city': 'Birmingham','state': 'AL'},
      'email': ['brandon.laidlaw@nosql-matters.org', 'laidlaw@nosql-matters.org'],
    'region': '205','phone': ['205-1673400', '205-0536483']},
    'likes': [],
  'memberSince': '2009-12-26'},

  {'name': {'first': 'Arturo','last': 'Kinatyan'},
    'gender': 'male',
    'birthday': '1962-01-18',
    'contact': {'address': {'street': '16 Last chance Run','zip': '86545','city': 'Rock Point','state': 'AZ'},
      'email': ['arturo.kinatyan@nosql-matters.org'],
    'region': '520','phone': []},
    'likes': ['chatting'],
  'memberSince': '2009-06-23'},

  {'name': {'first': 'Eddie','last': 'Colangelo'},
    'gender': 'male',
    'birthday': '1959-04-14',
    'contact': {'address': {'street': '16 Center Ln','zip': '12308','city': 'Schenectady','state': 'NY'},
      'email': ['eddie.colangelo@nosql-matters.org', 'colangelo@nosql-matters.org', 'eddie@nosql-matters.org'],
    'region': '518','phone': ['518-1453490']},
    'likes': [],
  'memberSince': '2012-03-09'},

  {'name': {'first': 'Margrett','last': 'Heartz'},
    'gender': 'female',
    'birthday': '1948-01-08',
    'contact': {'address': {'street': '18 Mintwood Way','zip': '53913','city': 'Baraboo','state': 'WI'},
      'email': ['margrett.heartz@nosql-matters.org'],
    'region': '608','phone': []},
    'likes': [],
  'memberSince': '2010-07-01'},

  {'name': {'first': 'Verna','last': 'Wigfield'},
    'gender': 'female',
    'birthday': '1973-02-14',
    'contact': {'address': {'street': '2 Main Blvd','zip': '71406','city': 'Belmont','state': 'LA'},
      'email': ['verna.wigfield@nosql-matters.org', 'wigfield@nosql-matters.org'],
    'region': '318','phone': ['318-5784855', '318-8625864']},
    'likes': [],
  'memberSince': '2010-07-22'},

  {'name': {'first': 'Jarrod','last': 'Litwiler'},
    'gender': 'male',
    'birthday': '1988-12-27',
    'contact': {'address': {'street': '17 Walnut Dr','zip': '15824','city': 'Brockway','state': 'PA'},
      'email': ['jarrod.litwiler@nosql-matters.org', 'jarrod@nosql-matters.org'],
    'region': '814','phone': ['814-7395320', '814-1794652']},
    'likes': ['running'],
  'memberSince': '2010-09-23'},

  {'name': {'first': 'Kelley','last': 'Wala'},
    'gender': 'female',
    'birthday': '1954-07-17',
    'contact': {'address': {'street': '21 Neosho Ln','zip': '74129','city': 'Tulsa','state': 'OK'},
      'email': ['kelley.wala@nosql-matters.org', 'kelley@nosql-matters.org'],
    'region': '918','phone': ['918-1081617', '918-4968318']},
    'likes': [],
  'memberSince': '2009-05-25'},

  {'name': {'first': 'Shaunta','last': 'Geringer'},
    'gender': 'female',
    'birthday': '1984-05-04',
    'contact': {'address': {'street': '15 Spring Run','zip': '18627','city': 'Lehman','state': 'PA'},
      'email': ['shaunta.geringer@nosql-matters.org', 'shaunta@nosql-matters.org'],
    'region': '570','phone': ['570-4850697']},
    'likes': ['snowboarding', 'travelling'],
  'memberSince': '2009-07-07'},

  {'name': {'first': 'Ira','last': 'Rechel'},
    'gender': 'male',
    'birthday': '1968-11-17',
    'contact': {'address': {'street': '9 Riggs Pl','zip': '23868','city': 'Lawrenceville','state': 'VA'},
      'email': ['ira.rechel@nosql-matters.org'],
    'region': '804','phone': ['804-3225311', '804-7803572']},
    'likes': ['running', 'shopping', 'skiing'],
  'memberSince': '2008-07-14'},

  {'name': {'first': 'Concetta','last': 'Rotondo'},
    'gender': 'female',
    'birthday': '1992-09-19',
    'contact': {'address': {'street': '9 19th Ave','zip': '65580','city': 'Vichy','state': 'MO'},
      'email': ['concetta.rotondo@nosql-matters.org'],
    'region': '573','phone': []},
    'likes': [],
  'memberSince': '2011-11-10'},

  {'name': {'first': 'Georgetta','last': 'Kolding'},
    'gender': 'female',
    'birthday': '1958-12-20',
    'contact': {'address': {'street': '22 290th St','zip': '44645','city': 'Marshallville','state': 'OH'},
      'email': ['georgetta.kolding@nosql-matters.org', 'kolding@nosql-matters.org'],
    'region': '330','phone': ['330-6429454']},
    'likes': ['running', 'boxing', 'reading'],
  'memberSince': '2011-12-08'},

  {'name': {'first': 'Rickie','last': 'Bakaler'},
    'gender': 'female',
    'birthday': '1990-09-22',
    'contact': {'address': {'street': '20 14th St','zip': '67761','city': 'Wallace','state': 'KS'},
      'email': ['rickie.bakaler@nosql-matters.org'],
    'region': '785','phone': ['785-8179421', '785-2447694']},
    'likes': ['biking'],
  'memberSince': '2008-01-11'}
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief regions
// //////////////////////////////////////////////////////////////////////////////

var regions = {
  '205': 'Greenland',
  '207': 'Middle West',
  '208': 'Yukon Territory',
  '212': 'Australian Capital Territory',
  '215': 'Madagascar',
  '252': 'Earth',
  '253': 'Sint Eustatius',
  '304': 'South Australia',
  '309': 'Arizona',
  '310': 'Samoa',
  '314': 'East (U.S.)',
  '315': 'Tonga',
  '316': 'Seychelles',
  '318': 'Albania',
  '320': 'Rocky Mountains',
  '330': 'Tasmania',
  '402': 'Jamaica',
  '406': 'Guadeloupe',
  '410': 'Connecticut',
  '415': 'North Pacific Ocean',
  '419': 'Belize',
  '423': 'Johnston Island',
  '501': 'Canada',
  '503': 'Haiti',
  '505': 'Solar system',
  '508': 'Réunion',
  '515': 'Cook Islands',
  '518': 'Cayman Islands',
  '520': 'Iowa',
  '540': 'American Samoa',
  '562': 'Ecuador',
  '570': 'Montserrat',
  '573': 'Spratly Islands',
  '580': 'Arab countries',
  '605': 'Eastern Hemisphere',
  '606': 'Gabon',
  '607': 'Victoria',
  '608': 'Ontario',
  '612': 'Barbados',
  '626': 'Yemen (Republic)',
  '636': 'Canal Zone',
  '660': 'Nova Scotia',
  '661': 'Alps',
  '662': 'Pyrenees',
  '702': 'Trinidad and Tobago',
  '706': 'Guinea',
  '712': 'Red Sea',
  '713': 'Saint Kitts and Nevis',
  '716': 'United States Miscellaneous Pacific Islands',
  '718': 'Central America',
  '719': 'California',
  '732': 'England',
  '760': 'Cocos (Keeling) Islands',
  '765': 'South China Sea',
  '775': 'Leeward Islands (West Indies)',
  '781': 'New South Wales',
  '785': 'Mississippi',
  '787': 'Georgia',
  '802': 'Tuvalu',
  '804': 'Southwest, New',
  '810': 'Paraguay',
  '812': 'Neptune',
  '813': 'Antigua and Barbuda',
  '814': 'Dominica',
  '830': 'Ghana',
  '831': 'New Brunswick',
  '860': 'Mediterranean Region; Mediterranean Sea',
  '870': 'Mercury',
  '903': 'Portugal',
  '908': 'Arctic Ocean; Arctic regions',
  '909': 'New York',
  '914': 'Himalaya Mountains',
  '915': 'Poland',
  '918': 'Newfoundland and Labrador'
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a users collection
// //////////////////////////////////////////////////////////////////////////////

exports.createUsers = function (name) {
  var i;

  var collection = db._collection(name);

  if (collection === null) {
    collection = db._create(name);
  } else {
    collection.truncate();
  }

  for (i = 0;  i < data.length;  ++i) {
    collection.save(data[i]);
  }

  return collection;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a regions collection
// //////////////////////////////////////////////////////////////////////////////

exports.createRegions = function (name) {
  var i;

  var collection = db._collection(name);

  if (collection === null) {
    collection = db._create(name);
  } else {
    collection.truncate();
  }

  for (i in regions) {
    if (regions.hasOwnProperty(i)) {
      collection.save({ region: i, name: regions[i]});
    }
  }

  return collection;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates locations for users
// //////////////////////////////////////////////////////////////////////////////

exports.createLocations = function (name, usersCollection, limit) {
  var i;

  if (limit === null || limit === undefined) {
    limit = usersCollection.count();
  }

  var collection = db._collection(name);

  if (collection === null) {
    collection = db._create(name);
  } else {
    collection.truncate();
  }

  collection.ensureIndex({ type: 'geo', fields: [ 'location' ] });

  var cursor = usersCollection.all();
  var lat = 59;
  var lon = 18;

  for (i = 0;  cursor.hasNext() && i < limit;  ++i) {
    var doc = cursor.next();

    var la = lat + (Math.random() - 0.5);
    var lo = lon + (Math.random() - 0.5);

    collection.save({ userId: doc._key, location: [la, lo] });
  }

  return collection;
};
