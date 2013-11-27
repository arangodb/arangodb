/* To avoid CSS expressions while still supporting IE 7 and IE 6, use this script */
/* The script tag referring to this file must be placed before the ending body tag. */

/* Use conditional comments in order to target IE 7 and older:
	<!--[if lt IE 8]><!-->
	<script src="ie7/ie7.js"></script>
	<!--<![endif]-->
*/

(function() {
	function addIcon(el, entity) {
		var html = el.innerHTML;
		el.innerHTML = '<span style="font-family: \'arangodb\'">' + entity + '</span>' + html;
	}
	var icons = {
		'icon_arangodb_youtube': '&#xe600;',
		'icon_arangodb_wireless': '&#xe601;',
		'icon_arangodb_windows3': '&#xe602;',
		'icon_arangodb_windows2': '&#xe603;',
		'icon_arangodb_windows': '&#xe604;',
		'icon_arangodb_web': '&#xe605;',
		'icon_arangodb_warning': '&#xe606;',
		'icon_arangodb_vimeo': '&#xe607;',
		'icon_arangodb_unpacked': '&#xe608;',
		'icon_arangodb_unlocked': '&#xe609;',
		'icon_arangodb_unhappy': '&#xe60a;',
		'icon_arangodb_twitter': '&#xe60b;',
		'icon_arangodb_trash': '&#xe60c;',
		'icon_arangodb_star': '&#xe60d;',
		'icon_arangodb_stapel2': '&#xe60e;',
		'icon_arangodb_stapel': '&#xe60f;',
		'icon_arangodb_snapshot': '&#xe610;',
		'icon_arangodb_shield': '&#xe611;',
		'icon_arangodb_settings2': '&#xe612;',
		'icon_arangodb_settings1': '&#xe613;',
		'icon_arangodb_search': '&#xe614;',
		'icon_arangodb_roundplus': '&#xe615;',
		'icon_arangodb_roundminus': '&#xe616;',
		'icon_arangodb_removefolder': '&#xe617;',
		'icon_arangodb_removedoc': '&#xe618;',
		'icon_arangodb_removedatabase': '&#xe619;',
		'icon_arangodb_power': '&#xe61a;',
		'icon_arangodb_pillow': '&#xe61b;',
		'icon_arangodb_performance': '&#xe61c;',
		'icon_arangodb_pencil': '&#xe61d;',
		'icon_arangodb_packed': '&#xe61e;',
		'icon_arangodb_note': '&#xe61f;',
		'icon_arangodb_mark': '&#xe620;',
		'icon_arangodb_map': '&#xe621;',
		'icon_arangodb_mail': '&#xe622;',
		'icon_arangodb_locked': '&#xe623;',
		'icon_arangodb_letter': '&#xe624;',
		'icon_arangodb_lamp': '&#xe625;',
		'icon_arangodb_info': '&#xe626;',
		'icon_arangodb_infinite': '&#xe627;',
		'icon_arangodb_import': '&#xe628;',
		'icon_arangodb_heart': '&#xe629;',
		'icon_arangodb_happ': '&#xe62a;',
		'icon_arangodb_gps': '&#xe62b;',
		'icon_arangodb_google': '&#xe62c;',
		'icon_arangodb_folder': '&#xe62d;',
		'icon_arangodb_flag': '&#xe62e;',
		'icon_arangodb_filter': '&#xe62f;',
		'icon_arangodb_favorite': '&#xe630;',
		'icon_arangodb_facebook': '&#xe631;',
		'icon_arangodb_export': '&#xe632;',
		'icon_arangodb_euro': '&#xe633;',
		'icon_arangodb_edit': '&#xe634;',
		'icon_arangodb_edge5': '&#xe635;',
		'icon_arangodb_edge4': '&#xe636;',
		'icon_arangodb_edge3': '&#xe637;',
		'icon_arangodb_edge2': '&#xe638;',
		'icon_arangodb_edge1': '&#xe639;',
		'icon_arangodb_dotted2': '&#xe63a;',
		'icon_arangodb_dotted': '&#xe63b;',
		'icon_arangodb_dollar': '&#xe63c;',
		'icon_arangodb_document2': '&#xe63d;',
		'icon_arangodb_docs': '&#xe63e;',
		'icon_arangodb_doc': '&#xe63f;',
		'icon_arangodb_database1': '&#xe640;',
		'icon_arangodb_compass': '&#xe641;',
		'icon_arangodb_checklist': '&#xe642;',
		'icon_arangodb_checked': '&#xe643;',
		'icon_arangodb_chart4': '&#xe644;',
		'icon_arangodb_chart3': '&#xe645;',
		'icon_arangodb_chart2': '&#xe646;',
		'icon_arangodb_chart1': '&#xe647;',
		'icon_arangodb_bubble3': '&#xe648;',
		'icon_arangodb_bubble2': '&#xe649;',
		'icon_arangodb_bubble1': '&#xe64a;',
		'icon_arangodb_box3': '&#xe64b;',
		'icon_arangodb_box2': '&#xe64c;',
		'icon_arangodb_box1': '&#xe64d;',
		'icon_arangodb_books': '&#xe64e;',
		'icon_arangodb_bookmark': '&#xe64f;',
		'icon_arangodb_attachment': '&#xe650;',
		'icon_arangodb_arrowright': '&#xe651;',
		'icon_arangodb_arrowleft': '&#xe652;',
		'icon_arangodb_arrowenter': '&#xe653;',
		'icon_arangodb_arrow10': '&#xe654;',
		'icon_arangodb_arrow9': '&#xe655;',
		'icon_arangodb_arrow8': '&#xe656;',
		'icon_arangodb_arrow7': '&#xe657;',
		'icon_arangodb_arrow6': '&#xe658;',
		'icon_arangodb_arrow5': '&#xe659;',
		'icon_arangodb_arrow4': '&#xe65a;',
		'icon_arangodb_arrow3': '&#xe65b;',
		'icon_arangodb_arrow2': '&#xe65c;',
		'icon_arangodb_arrow1': '&#xe65d;',
		'icon_arangodb_addfolder': '&#xe65e;',
		'icon_arangodb_adddoc': '&#xe65f;',
		'icon_arangodb_adddatabase': '&#xe660;',
		'0': 0
		},
		els = document.getElementsByTagName('*'),
		i, c, el;
	for (i = 0; ; i += 1) {
		el = els[i];
		if(!el) {
			break;
		}
		c = el.className;
		c = c.match(/icon_[^\s'"]+/);
		if (c && icons[c[0]]) {
			addIcon(el, icons[c[0]]);
		}
	}
}());
