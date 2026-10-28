$code = @"
using System; using System.IO; using System.Collections.Generic; using System.Globalization;
public class Section145 {
 public static void Run(string path,string output) {
 System.Threading.Thread.CurrentThread.CurrentCulture=CultureInfo.InvariantCulture; var vs=new List<double[]>(); var hits=new SortedSet<string>(); var rows=new List<string>();
 var pts=new List<double[]>(); for(double x=2104;x<=2146;x+=1)for(double y=1882.5;y<=1887;y+=1.5)pts.Add(new double[]{x,y});
 var zs=new List<double>[pts.Count]; for(int j=0;j<zs.Length;j++)zs[j]=new List<double>();
 foreach(var line in File.ReadLines(path)) {var t=line.Split(new[]{' '},StringSplitOptions.RemoveEmptyEntries); if(t.Length<4)continue;
 if(t[0]=="v")vs.Add(new[]{double.Parse(t[1],CultureInfo.InvariantCulture),double.Parse(t[2],CultureInfo.InvariantCulture),double.Parse(t[3],CultureInfo.InvariantCulture)});
 if(t[0]!="f")continue; var a=vs[int.Parse(t[1])-1]; var b=vs[int.Parse(t[2])-1];var c=vs[int.Parse(t[3])-1];
 double d=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);if(Math.Abs(d)<1e-9)continue;
 for(int j=0;j<pts.Count;j++){double x=pts[j][0],y=pts[j][1]; if(x<Math.Min(a[0],Math.Min(b[0],c[0]))||x>Math.Max(a[0],Math.Max(b[0],c[0]))||y<Math.Min(a[1],Math.Min(b[1],c[1]))||y>Math.Max(a[1],Math.Max(b[1],c[1])))continue;
 double u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/d;double v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/d;if(u< -1e-5||v< -1e-5||u+v>1.00001)continue;
 double z=u*a[2]+v*b[2]+(1-u-v)*c[2]; if(z>=89&&z<=108)zs[j].Add(Math.Round(z,2)); }
 }
 rows.Add("x\ty\tgeometry_intersections_z_unclassified");for(int j=0;j<pts.Count;j++){var z=new SortedSet<double>(zs[j]);rows.Add(pts[j][0].ToString(CultureInfo.InvariantCulture)+"\t"+pts[j][1].ToString(CultureInfo.InvariantCulture)+"\t"+string.Join(",",z));} File.WriteAllLines(output,rows);
 }
}
"@
Add-Type -TypeDefinition $code
[Section145]::Run((Join-Path $pwd 'build/coordinator-c145/Shore.US.exact-subset.obj'),(Join-Path $pwd 'build/coordinator-c145/US-canonical-sections.tsv'))

