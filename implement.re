= インタプリタの実装

どんな形のものを作るのかが大体見えてきたところで、ぼちぼち実装にとりかかりましょう。
といっても、紙幅の都合もあるため、全ソースコードを掲載というわけにはいきません。
要点となる部分だけをかいつまんで説明したいと思います。
完全なソースコードはまえがきにて触れたとおりGitHubにて公開しているので、そちらからご覧ください。

== プロジェクトの構成

はじめに、このプロジェクトの構成について説明します。
まずプロジェクト名であり、最終的に出来上がるインタプリタのバイナリファイル名として「sqe」という名前を付けることにします。
sqeを構成するファイルは次のようになっています。

: app.cpp, app.h
    アプリケーションクラス
: sqdlfcn.cpp, sqdlfcn.h
    dlfcn のラッパー関数
: libsc.cpp, libsc.h
    require の実装
: loadmap.cpp, loadmap.h
    ロード済みSELの管理
: loadpath.cpp, loadpath.h
    SELのファイルシステム上のパスを特定するためのルーチン
: main.cpp
    アプリケーションのエントリポイント
: version.h
    バージョン情報などの定数定義


== requireを実装する

=== requireネイティブクロージャの実装

requireは実際には単なるSquirrelのネイティブクロージャとして実装されます。
実装はプログラム@<list>{require_import}のような感じになっています。
大雑把には「受け取った引数keyをもとに、Squirrelスクリプト（.nut拡張子を持つファイル）、もしくはSELをロードする」という処理を行なっています。

//list[require_import][sqe/libsc.cpp, requireの実装]{
static SQInteger _sc_require (HSQUIRRELVM vm)
{
  // 引数としてテーブルが渡されている場合はそれを取り出し、
  // そうでなければ予め RootTable を Push しておく
  SQInteger args = sq_gettop (vm);
  switch( args ) {
  case 2:
    sq_pushroottable (vm);
      break;
  default:
    break;
  }

  for(;;) // breakable
  {
    // モジュールのパス名を取得
    const SQChar *path = 0;
    if( SQ_FAILED( sq_getstring (vm, -2, &path) ) ) { 
      break;
    }

    // スタック上のテーブルオブジェクトを取得
    if( sq_gettype (vm, -1 ) != OT_TABLE ) {
      scprintf (_SC( "argument is not TABLE\n" ));
      break;
    }
    HSQOBJECT table;
    sq_getstackobj (vm, -1, &table);
    sq_addref (vm, &table);

    // スタックをクリアし、ターゲットテーブルをスタックに Push
    sq_settop (vm, 0);
    sq_pushobject (vm, table);

    // 補完されたパスを生成しつつ順番に調べていく
    SQChar *pathComplete = 0;
    SCImportPathGenerator *generator = 
    SCImportPathGenerator::createInstance ();
    if( 0 == generator ) {
      break;
    }
    // 生成された補完済みパスがすでに読み込まれているのかチェックしつつ、
    // 生成済みでないならそれをロードする
    generator->set (path);
    while( (pathComplete = generator->get ()) )
    {
      if( _sc_check_loaded_file (vm, pathComplete) ) {
        break;
      }
      else
      if( generator->getEntryType () == SC_ENTRY_TYPE_NUT ) {
        if( SQ_SUCCEEDED( _sc_import_nut (vm, pathComplete) ) ) {
          break;
        }
      }
      else
      if( generator->getEntryType () == SC_ENTRY_TYPE_EXTEND ) {
        if( SQ_SUCCEEDED( _sc_import_extend (vm, pathComplete) ) ) {
          break;
        }
      }
    }
    // 補完パスの生成器の終了
    if( generator ) {
      SCImportPathGenerator::deleteInstance (generator);
    }

    sq_settop (vm, 0);
    sq_pushobject (vm, table);
    break;
  }

  // require はテーブルオブジェクトを返す
  return 1;
}
//}

これはプログラム@<list>{require_binding}のような処理でSquirrel VMにバインディングされます。
この部分は特別に解説するところのない、Squirrelのネイティブクロージャのバインディング処理です。

//list[require_binding][sqe/libsc.cpp, ネイティブクロージャrequireのバインド]{
void sc_register_sclib (HSQUIRRELVM vm)
{
  sq_pushstring (vm, _SC( "require" ), -1);
  sq_newclosure (vm, _sc_require, 0);
  sq_setnativeclosurename (vm, -1, _SC( "_sc_require" ));
  if( SQ_FAILED( sq_newslot (vm, -3, SQFalse) ) ) {
    sq_getlasterror (vm);
    const SQChar *error = 0;
    if( SQ_SUCCEEDED( sq_getstring (vm, -1, &error) ) ) {
      scprintf (_SC( "Error [%s]\n" ), error);
    }
  }
}
//}

sc_register_sclib関数は、アプリケーションの初期化処理で呼び出されます（@<list>{appinit}）。
これにより、sqe上で実行されたSquirrelスクリプトは、暗黙的にrequireを呼び出すことができるようになります。
sqeの実装では、Applicationというクラスがエントリポイント直下でインスタンス化され、Application::initが呼び出されます。
sc_register_sclib 関数の呼び出しは、このApplication::initの内部で（その他の Squirrel 標準ライブラリのバインディング関数の呼び出しとともに）行われています。

//list[appinit][sqe/app.cpp アプリケーションの初期化処理]{
void Application::init (void)
{
  // VM initialize
  vm = sq_open (1024);
  sq_setprintfunc (
    this->vm,
    printfunc,
    errorfunc
  );
  sq_pushroottable         (this->vm);
  sqstd_register_bloblib   (this->vm);
  sqstd_register_iolib     (this->vm);
  sqstd_register_systemlib (this->vm);
  sqstd_register_mathlib   (this->vm);
  sqstd_register_stringlib (this->vm);
  sqstd_seterrorhandlers   (this->vm);
  sq_pop                   (this->vm, 1);

  sc_initialize_loadpath   ();
  sq_pushroottable         (this->vm);
  sc_register_sclib        (this->vm);
  sq_pop                   (this->vm, 1);
}
//}


=== SquirrelスクリプトとSELの探索の優先順位

require関数は、まず引数として受け取った文字列（key）に対応するSquirrelスクリプトが無いかどうかを探索し、対応するものが見つかったのであればこれを優先して読み込むようになっています。
このようにしているのは、Squirrelスクリプトとして実装したものも、SELとして実装したものも、特に意識することなく透過的にrequireできるようにするためです。
別の言い方をすれば、sq環境などでは標準的に使えるloadfile関数とほぼ同じような使い方も出来るようにするため、という事でもあります。

=== ファイル・システム上のパスを決める

require関数に渡された引数keyから推測されるファイルシステム上のエントリのパスを生成する処理を行うのが、SCImportPathGeneratorというクラスです。
@<list>{scimportpathgenerator} は_sc_require関数における処理のうち、このSCImportPathGeneratorクラスにまつわる部分だけをピックアップしたものです。

//list[scimportpathgenerator][sqe/libsc.cpp, SCImportPathGenerator]{
static SQInteger _sc_require (HSQUIRRELVM vm)
{
  for(;;) {
    /* ... */
    SQChar *pathComplete = 0;
    SCImportPathGenerator *generator = 
    SCImportPathGenerator::createInstance ();
    if( 0 == generator ) {
      break;
    }
    generator->set (key);
    while( (pathComplete = generator->get ()) )
    {
      /* ... */
    }
    // 補完パスの生成器の終了
    if( generator ) {
      SCImportPathGenerator::deleteInstance (generator);
    }
    /* ... */
  }
}
//}

SCImportPathGenerator::setメソッドを呼び出すと、内部でSquirrelスクリプトやSELのパスとなるであろう文字列を幾つか生成します。
それをSCImportPathGenerator::get で順番に取り出してロード処理を行います。
SCImportPathGenerator::getは生成した文字列が無くなるとnullを返すようになっているので、取り出せなくなるまで while 文によって処理を続けます。

SCImportPathGeneratorは内部的に、カレントディレクトリやホームディレクトリ、@<code>{/usr/lib}などのディレクトリを基準としたパスをいくつか生成します。
詳しい実装については本書では触れませんが、@<fn>{scimportpathgenerator_note}優先的に読み込みたい場所が順番に SCImportPathGenerator::getで取り出されるよう、生成処理を行なっています。

//footnote[scimportpathgenerator_note][煮詰める時間が無かった]

また、SCImportPathGenerator生成したパスが指しているものがSquirrelスクリプトなのか SEL なのかという事を示す値として、ScEntryType列挙定数に属するいずれかの値をパスとセットで生成します（@<table>{scentrytype}）。
これは後に、パスを元に実際にロードするという処理において用いられることになります。

//table[scentrytype][ScEntryType 列挙定数]{
列挙定数名							意味
-------------------------------------------------------------------------------
SC_ENTRY_TYPE_NUT			  Squirrel スクリプト
SC_ENTRY_TYPE_EXTEND		SEL
SC_ENTRY_TYPE_INVALID	  無効なタイプ
//}


=== 多重で読み込まれた場合の挙動を考える

require関数の実装は、require_once（一度読み込まれたモジュールは読み込まない）な挙動を示す実装になっています。
@<fn>{require_once_note} そのため、多重で同じモジュールが読み込まれようとした場合に、それをはじくために、読み込み済みのモジュールのパスとSquirrel VMを結びつけてものを読み込み済み情報として記録しています。

//footnote[require_once_note][余談ですが、ここはRuby言語の振る舞いを参考にしています]

これは具体的には、Entryクラス（@<list>{entry}）を定義し、さらにそれを@<code>{std::set <Entry*>}とすることによって行われます。
読み込みパスとSquirrel VMの組み合わせをEntryとし、重複しないようにstd::setコンテナによって管理します。

//list[entry][sqe/loadpath.cpp, Entryクラス]{
/**
 * @class 個々のロード済みモジュールを表す構造体
 */
struct Entry
{
public:
  Entry (HSQUIRRELVM vm, SQChar *path, void *handle) {
    this->vm = vm;
    this->path = path;
    this->handle = handle;
  }
  
  HSQUIRRELVM vm;  // vm
  SQChar *path;    // パス
  void *handle;    // 拡張ライブラリ用
};
//}

この読み込み済みのモジュールの管理を行うために実装されている関数が、@<table>{loadedfile} の関数です。
これらの関数は全て sqe/loadmap.cpp に実装されています。

//table[loadedfile][読み込み済みモジュール管理のための関数]{
関数名															処理
-------------------------------------------------------------------------------
_sc_check_loaded_file 関数					読み込み済みかどうかチェックする
_sc_append_loaded_file_extend 関数	SELを読み込み済みとして登録する
_sc_append_loaded_file_nut 関数			Squirrelスクリプトを読み込み済みとして登録する
//}

SCImportPathGenerator::getで取得したパスについて、_sc_check_loaded_file関数でチェックしてロード済みでなければロード処理を行います。
このとき、SCImportPathGenerator::getEntryType関数を使い、getで取得したパスが Squirrelスクリプトのものなのか、SELのものなのかを判定します。

=== Squirrelスクリプトの場合の処理

Squirrelスクリプトが読み込まれる場合は、Squirrelの標準的なインタプリタであるsqで使えるloadfile関数とほぼ同様の振る舞いを示します。
Squirrelスクリプトの場合の処理は、_sc_import_nutという関数で行われます。
実装はプログラム @<list>{sc_import_nut} のようになっています。

//list[sc_import_nut][sqe/libsc.cpp, _sc_import_nut関数の実装]{
static SQRESULT _sc_import_nut (HSQUIRRELVM vm, SQChar *path)
{
  // スクリプトファイルの読み込み
  if( SQ_FAILED( sqstd_loadfile (vm, path, SQTrue) ) ) { 
    return SQ_ERROR;
  }

  // sq_call 関数の設定
  sq_push (vm, -2);
  const SQRESULT callResult = sq_call (
    vm, 1,
    SQFalse,
    SQTrue
  );
  if( SQ_FAILED( callResult ) ) {
    const SQChar *error = 0;
    sq_getlasterror (vm);
    if( SQ_SUCCEEDED( sq_getstring (vm, -1, &error) ) ) {
      scprintf (_SC( "libsc ERROR : [%s]\n" ), error);
    }
    return SQ_ERROR;
  } 
  // 読み込みに成功しているのでロード済みファイルとして追加
  _sc_append_loaded_file_nut (vm, path);
  return SQ_OK;
}
//}

=== SELの場合の処理

keyから推測したSquirrelスクリプトがファイルシステム上に存在しない場合、次にSELが存在しないか探索されることになります。
該当するSELが存在する場合にはこれをロードします。
この処理は_sc_import_extend関数によって行われます（@<list>{sc_import_extend}）。

この関数では、SELのロードに成功した場合（sc_dlopenに成功した場合）、同時に「initialize」というシンボル名をSEL中から探索し、それを初期化関数としてコールするという処理を行なっています。
初期化関数などについては、SELの実装についてまとめて触れるという形で後述しているので、詳しくはそちらをご覧ください。

//list[sc_import_extend][sqe/libsc.cpp, _sc_import_extend関数の実装]{
static SQRESULT _sc_import_extend (HSQUIRRELVM vm, const SQChar *path) 
{
  // ライブラリの動的ローディング
  for(;;) {
    void *handle = sc_dlopen (const_cast <SQChar*> (path));
    if( !handle ) {
      scprintf (_SC( "%s\n" ), sc_dlerror ());
      break;
    } 
    
    // 初期化関数のシンボルを取得
    SQRESULT (*func) (HSQUIRRELVM) = (
      reinterpret_cast <SQRESULT (*) (HSQUIRRELVM)> (
        sc_dlsym (handle, _SC( "initialize" ))
      )
    );
    const SQChar* error = 0;
    if( (error = sc_dlerror ()) != NULL ) {
      scprintf (_SC( "%s\n" ), error);
      break;
    }
    else {
      // 初期化関数を実行
      if( SQ_FAILED( func (vm) ) ) {
        scprintf (_SC( "拡張ライブラリの初期化に失敗しました\n" ));
      }
    }

    // 読み込み済みファイルとして追加
    _sc_append_loaded_file_extend (vm, path, handle);
    break;
  }

  return SQ_ERROR;
}
//}

libdl系の関数は、sqe/sqdlfcn.hおよびsqe/sqdlfcn.cppに定義された関数で簡単にラップされています。
sqdlfcnではlibdlの関数全てをラップしているわけではありませんが、sqeが必要とするものは全てここに記述されています。
sqeは、全てこのラッパー群を経由して、libdl系の関数にアクセスしています。

_sc_import_extend関数で呼び出している、sc_dlopen、sc_dlsym、sc_dlerrorがsqdlfcn系の関数にあたります。
これらはそれぞれ、SEL（動的ロード可能なライブラリ）の「動的ロード」「シンボルの取得」「エラーの取得」を行う関数となっています。


== まとめ

このセクションでは、本書の要旨であるrequire関数の実装について説明しました。
一言でまとめてみると「require関数に与えられた引数keyを元に、所定の場所から SquirrelスクリプトもしくはSELをロードする。多重では読み込まない」という感じでしょうか。

さて、require関数が実装できたとしても、読み込むべきSquirrelスクリプトやSELがなければ意味がありません。
次のセクションでは、require 関数でロードすることのできるSquirrelスクリプトやSELの実装について、サンプルを交えて解説します。

#@# vim: ft=tex
#@# vim: wrap
