'use strict';

/**
 * @license
 * Copyright (c) 2014 Alistair G MacDonald
 * 
 * (The MIT License)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
 * documentation files (the 'Software'), to deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit 
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT 
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
  
  //Requires
  var c = require('js/contrib/flux/axel')
    , internal = require('internal')
    , int = parseInt
    , sin = Math.sin
    // , cos = Math.cos
    // , floor = Math.floor
    // , ceil = Math.ceil
    // , pow = Math.pow
    , score = 0
    , bullets = []
    , maxBullets = 20
    , bulletSpeed = 0.425
    , width = c.cols
    , height = c.rows
    , p1x = c.cols/2
    , p1y = c.rows-2
    , lp1x = p1x
    , lp1y = p1y
    , interval = 20
    , isEnabled = true
    , tick =0
    , enemies = []
    , maxEnemies = 24
    , enemySpeed = 0.025
    ;

    // var theBrush = '█';
    var theBrush = ' ';
  
  genEnemies();
  function genEnemies(){
    for (var y=0; y< c.rows; y+=3){
      for (var x=0; x< c.cols*0.75; x+=4){
        enemies.push({
          x: 2+x,
          y: y
        });
        if (enemies.length>=maxEnemies) {
          return;
        }
      }
    }
  }

  

  function shoot(){
    var newBullet = {
      x: p1x,
      y: p1y-3, 
      speed: bulletSpeed
    };

    bullets.push(newBullet);

    if(bullets.length>maxBullets){
      bullets.shift();
    }
  }

  function updateBullets(){
    bullets.forEach(function(bullet){
    
      // Set last positions
      bullet.lx = bullet.x;
      bullet.ly = bullet.y;
      
      // Move and accelarate
      bullet.y-=bullet.speed;
      bullet.speed+=bullet.speed*0.025;
    


      if( int(bullet.x)!==int(bullet.lx) ||
          int(bullet.y)!==int(bullet.ly))
        {
          // Draw off
          c.cursor.reset();
          c.brush = ' ';
          c.point(bullet.lx, bullet.ly);

          // Draw on
          c.brush = theBrush;
          c.bg(0,255,0);
          c.point(bullet.x, bullet.y);
      }

      function destroyBullet(){
        c.cursor.reset();
        c.brush = ' ';
        c.point(bullet.x, bullet.y);
        bullets.shift();
        return;
      }

      enemies.forEach(function(enemy,i){
        var D = c.dist(enemy.x, enemy.y, bullet.x, bullet.y);
        if(D<5){
          genExplosion(enemy.x,enemy.y);
          destroyBullet();
          enemies.splice(i,1);
          updateScore(12.34*bullet.speed);
        }
      });

      if(bullet.y<1){
        destroyBullet();
      }
      
    })
  }


  var explosions = [];
  function genExplosion(x,y){
    explosions.push({
      x:x,
      y:y,
      size:0,
      lsize:0,
      rate: 1,
      max: 5
    });
  }
  function updateExplosions(){
    explosions.forEach(function(exp){
      exp.lsize = exp.size;
      
      c.cursor.reset();
      c.brush = ' ';
      c.circ(exp.x, exp.y, exp.lsize);

      c.bg(255,128,0);
      c.brush = theBrush;
      c.circ(exp.x, exp.y, exp.size);

      exp.size+=exp.rate;
      if(exp.size>exp.max){
        c.cursor.reset();
        c.circ(exp.x, exp.y, exp.lsize);
        explosions.shift();
      }
    });
  }

  function updateEnemies(){
    enemies.forEach(function(enemy){
      enemy.ly = enemy.y;
      enemy.lx = enemy.x;
      enemy.y+=enemySpeed;
      enemy.x=enemy.x+(sin(tick/10)/1.5);
     
      // Only draw enemies again if they have moved
      if(int(enemy.y)!==int(enemy.ly) ||
          int(enemy.x)!==int(enemy.lx)) 
        {
          c.cursor.reset();
          c.brush = ' ';
          drawEnemy(int(enemy.lx), int(enemy.ly));

          c.bg(255,0,0);
          c.brush = theBrush;
          drawEnemy(int(enemy.x), int(enemy.y));  
      }
    });
  }


  function updateScore(add){
    score+=add;
    c.cursor.reset();
    c.fg(255,255,255);
    c.text(0, c.rows, "Score: "+ int(score));
  }

  function drawPlayer(x, y){
    c.brush = theBrush;
    c.bg(0,255,0);
    c.line(x-2, y, x+2, y);  
    c.line(x, y, x, y-3);  
    c.line(x-2, y, x-2, y-2);  
    c.line(x+2, y, x+2, y-2);  
  }

  function erasePlayer(x, y){
    c.brush = ' ';
    c.cursor.reset();
    c.line(x-2, y, x+2, y);  
    c.line(x, y, x, y-3);  
    c.line(x-2, y, x-2, y-2);  
    c.line(x+2, y, x+2, y-2);  
  }

  function drawEnemy(x,y){
    c.line(x-1, y, x+1, y);
    c.line(x-1, y, x-1, y+2);
    c.line(x+1, y, x+1, y+2);
  }


  function eachLoop(){
    tick+=1;

    width = c.cols;
    height = c.rows;

    pollKeyboard();
  
    updateBullets();
    updateEnemies();
    updateExplosions();
    checkKeyDown();

    erasePlayer(lp1x,lp1y);
    drawPlayer(p1x,p1y);
  }

  
  function endGame(){
    isEnabled = false;
    c.cursor.on();
    c.cursor.restore();
    internal.print(" Good Bye!");
  }


  function start(){
    c.cursor.off();
    c.clear();
    c.scrub(1, 1, c.cols, c.rows);
    while (isEnabled) {
      eachLoop();
      internal.sleep(1 / 45);
    }
  }


  start();


  function left(){
    lp1x = p1x;
    p1x-=p1x>4?1:0;
  }
  function right(){
    lp1x = p1x;
    p1x+=p1x<width-4?1:0;
  }


  //// KEYBOARD EVENTS ///////////////////////////////////////////////////////


  var keyDown = null;
  var lastChecked = now();
  var releaseTime = 25;

  function now(){
    return (+new Date());
  };


var dir;
  function checkKeyDown(){
    // if (now()-lastChecked>30){
    //   keyDown =null;
    // }

    switch(dir){
    case 'left':
      left();
      break;
    case 'right':
      right();
      break;
    }

    lastChecked = now();
  }

function pollKeyboard() {
  var code = internal.pollStdin();

  if (code) {
    if (code == 27) endGame();// esc
    if (code == 3) endGame();// esc
    if (code == 113 || code == 81) endGame();// q
    if (code == 97 || code == 65 || code == 37) dir = 'left';// a
    if (code == 100 || code == 68 || code == 39) dir = 'right';// d
    if (code == 32 || code == 119 || code == 38) shoot();// w / space
  }
}
