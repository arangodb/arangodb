(function($) {
$.fn.pagination = function(o) {
	var prevText = o.prevText ? o.prevText : '<<';
	var nextText = o.nextText ? o.nextText : '>>';
	var moreText = o.moreText ? o.moreText :'...';
	var isDisplayFirst = o.isDisplayFirst || true;
	var isDisplayLast = o.isDisplayLast || true;
	var current = o.current ? o.current : 1;
	var count = o.count ? o.count : 0;
	var link = o.link ? o.link : '#?page={p}';
	var displayCount = o.displayCount ? o.displayCount : 7;
	
	var me = $(this);

console.log(current);

	me.initPagination = function () {
		// 创建 ul
		var ul = $('<ul>');
		// 获取 页码数组
		var disp = me.computeDisplayPageNumber();
		for(var i=0;i<disp.length;i++){
			// 创建 li
			var li = $('<li>');
			// 创建 a
			var a = $("<a>");
			// 计算超链接中的页码
			var p=0;
			if(disp[i]==moreText){
				if(i==2){
					p=disp[i+1]-1;
				}else if(i==displayCount|| i==displayCount+1){
					p=disp[i-1]+1;
				}
			}else if(disp[i]==prevText){
				p = current-1;
			}else if(disp[i]==nextText){
				p = current+1;
			}else{
				p = disp[i];
			}
			// 超链接
			var pLink = link.replace('{p}',p);
			// 设置a的超链接和显示文本
			a.attr("href",pLink).text(disp[i]);
			// 为li添加a
			li.append(a);
			// 如果是当前页，设置样式
			if(current ==disp[i]){
				li.addClass('active');
			}
			// 向 ul 添加这个li
			ul.append(li);
		}
		// 添加ul
		me.append(ul);
	};
	
	me.computeDisplayPageNumber = function(){
		var disp = new Array();
		var j = 0;
		// 如果当前页不是第一页，则添加 prevText
		if(current>1){
			disp[j] = prevText;
			j++;
		}
		// 添加具体页码的逻辑
		if(count <= 1){
		// 如果总页数小于等于1，不添加任何页码
			return disp;
		}else if(count <= displayCount){
		// 如果总页数小于显示页数，则添加页码——1至count
			for(var i=1;i<=count;i++){
				disp[j] = i;
				j++;
			}
		}else if(displayCount - current >=2){
		// (当前页在前端)如果显示页数 - 当前页 大于等于2，则添加页码——1至displayCount，后面再添加一个moreText，后面再添加一个末页页码
			for(var i=1;i<displayCount;i++){
				disp[j] = i;
				j++;
			}
			disp[j] = moreText;
			j++;
			if(isDisplayLast){
				disp[j] = count;
				j++;
			}
		}else if(count - current <= (displayCount-1)/2){
		// (当前页在后端)如果总页数 - 当前页 小于等于(displayCount-1)/2，则先添加一个首页页码，再添加一个moreText，再添加页码——count-(displayCount-2)至count
			if(isDisplayFirst){
				disp[j] = 1;
				j++;
			}
			disp[j] = moreText;
			j++;
			for(var i=1;i<displayCount;i++){
				disp[j] = count - (displayCount -i-1);
				j++;
			}
		}else{
		// (当前页在中间)其他的情况，则先添加一个首页页码，再添加一个moreText，再添加当前页左边(displayCount-1)/2-1个页码，再添加当前页码，再添加一个moreText，再添加末页页码
			if(isDisplayFirst){
				disp[j] = 1;
				j++;
			}
			disp[j] = moreText;
			j++;
			var alignLeftCount = (displayCount-1)/2-1;
			for(var i=0;i<alignLeftCount;i++){
				disp[j] = current-alignLeftCount+i;
				j++;
			}
			disp[j] = current;
			j++;
			var alignRightCount = alignLeftCount;
			for(var i=0;i<alignRightCount;i++){
				disp[j] = current+i+1;
				j++;
			}
			disp[j] = moreText;
			j++;
			if(isDisplayLast){
				disp[j] = count;
				j++;
			}
		}
		// 如果当前页不是最后一页，则添加 nextText
		if(current != count){
			disp[j] = nextText;
			j++;
		}
		return disp;
	};
	
	me.initPagination();
};
})(jQuery);
